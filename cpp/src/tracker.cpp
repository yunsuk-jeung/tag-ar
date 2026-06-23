#include <algorithm>
#include <cerrno>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Eigen/Cholesky>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/video/tracking.hpp>

#include "tagar/tracker.hpp"
#include "tagar/frame.hpp"
#include "tagar/logger.hpp"
#include "tagar/one_euro_filter.hpp"

namespace tagar {

namespace {
std::array<float, 16> ToPoseArray(const Sophus::SE3d& T) {
  Eigen::Matrix4f m = T.matrix().cast<float>();
  std::array<float, 16> out{};
  std::copy(m.data(), m.data() + 16, out.begin());
  return out;
}

cv::aruco::DetectorParameters MakeDetectorParams() {
  cv::aruco::DetectorParameters p;
  p.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;
  return p;
}

Sophus::SE3d CamFromTag(const cv::Mat& rvec, const cv::Mat& tvec) {
  const Eigen::Vector3d omega(rvec.at<double>(0), rvec.at<double>(1),
                              rvec.at<double>(2));
  const Eigen::Vector3d t(tvec.at<double>(0), tvec.at<double>(1),
                          tvec.at<double>(2));
  return Sophus::SE3d(Sophus::SO3d::exp(omega), t);
}

double DepthScore(const Sophus::SE3d& T_c_t,
                  const std::vector<cv::Point3f>& corners_3d,
                  const std::vector<cv::Point2f>& corners, const Frame& frame) {
  double err = 0.0;
  int n = 0;
  for (size_t k = 0; k < corners_3d.size(); ++k) {
    const Eigen::Vector3d p_t(corners_3d[k].x, corners_3d[k].y,
                              corners_3d[k].z);
    const Eigen::Vector3d p_c = T_c_t * p_t;
    const float measured = frame.DepthAt(corners[k].x, corners[k].y);
    if (measured <= 0.0f) {
      continue;
    }
    const double d = p_c.z() - static_cast<double>(measured);
    err += d * d;
    ++n;
  }
  return n > 0 ? err / n : std::numeric_limits<double>::infinity();
}

struct DepthObservation {
  double u, v, z;
};

std::vector<DepthObservation> SampleQuadDepth(
    const Frame& frame, const std::vector<cv::Point2f>& quad, int max_samples) {
  std::vector<DepthObservation> depth_obs;
  if (!frame.HasDepth() || quad.size() != 4) {
    return depth_obs;
  }
  depth_obs.reserve(max_samples);

  float min_x = quad[0].x, max_x = quad[0].x;
  float min_y = quad[0].y, max_y = quad[0].y;
  for (const cv::Point2f& c : quad) {
    min_x = std::min(min_x, c.x);
    max_x = std::max(max_x, c.x);
    min_y = std::min(min_y, c.y);
    max_y = std::max(max_y, c.y);
  }

  const double area =
      std::max(1.0, static_cast<double>(max_x - min_x) * (max_y - min_y));
  const int stride =
      std::max(1, static_cast<int>(std::sqrt(area / std::max(1, max_samples))));
  for (int v = static_cast<int>(std::floor(min_y));
       v <= static_cast<int>(std::ceil(max_y)); v += stride) {
    for (int u = static_cast<int>(std::floor(min_x));
         u <= static_cast<int>(std::ceil(max_x)); u += stride) {
      if (cv::pointPolygonTest(quad, cv::Point2f(u, v), false) < 0) {
        continue;
      }
      const float z =
          frame.DepthAt(static_cast<float>(u), static_cast<float>(v));
      if (z > 0.0f) {
        depth_obs.push_back({static_cast<double>(u), static_cast<double>(v),
                             static_cast<double>(z)});
      }
    }
  }
  return depth_obs;
}

Sophus::SE3d RefinePose(const Sophus::SE3d& T_c_t, const Frame& frame,
                        const std::vector<cv::Point3f>& corners_3d,
                        const std::vector<cv::Point2f>& corners_obs,
                        const std::vector<DepthObservation>& depth_obs,
                        double sigma_px, double sigma_z, double huber,
                        int max_iters) {
  const double fx = frame.GetFx(), fy = frame.GetFy();
  const double cx = frame.GetCx(), cy = frame.GetCy();
  const double kHuber = huber;  // depth robust threshold [m]
  const double w_px = 1.0 / (sigma_px * sigma_px);
  const double w_z = 1.0 / (sigma_z * sigma_z);

  Sophus::SE3d T = T_c_t;
  double rms_px0 = 0.0, rms_z0 = 0.0;
  double rms_px = 0.0, rms_z = 0.0;
  int n_px = 0, n_z = 0, iters = 0;
  for (int it = 0; it < max_iters; ++it) {
    const Eigen::Matrix3d R = T.rotationMatrix();
    const Eigen::Vector3d n = R.col(2);
    const Eigen::Vector3d t = T.translation();

    Eigen::Matrix<double, 6, 6> H = Eigen::Matrix<double, 6, 6>::Zero();
    Eigen::Matrix<double, 6, 1> b = Eigen::Matrix<double, 6, 1>::Zero();
    double sse_px = 0.0, sse_z = 0.0;
    n_px = 0;
    n_z = 0;

    // Four-corner reprojection.
    for (size_t k = 0; k < corners_3d.size(); ++k) {
      const Eigen::Vector3d P =
          T *
          Eigen::Vector3d(corners_3d[k].x, corners_3d[k].y, corners_3d[k].z);
      if (P.z() <= 0.0) {
        continue;
      }
      const double inv_z = 1.0 / P.z();
      const Eigen::Vector2d r(fx * P.x() * inv_z + cx - corners_obs[k].x,
                              fy * P.y() * inv_z + cy - corners_obs[k].y);
      Eigen::Matrix<double, 2, 3> dproj;
      dproj << fx * inv_z, 0.0, -fx * P.x() * inv_z * inv_z,  //
          0.0, fy * inv_z, -fy * P.y() * inv_z * inv_z;
      Eigen::Matrix<double, 3, 6> dPdxi;
      dPdxi.leftCols<3>() = Eigen::Matrix3d::Identity();
      dPdxi.rightCols<3>() = -Sophus::SO3d::hat(P);
      const Eigen::Matrix<double, 2, 6> J = dproj * dPdxi;
      H += w_px * J.transpose() * J;
      b += w_px * J.transpose() * r;
      sse_px += r.squaredNorm();
      ++n_px;
    }

    // Depth residual.
    for (const DepthObservation& s : depth_obs) {
      const Eigen::Vector3d d((s.u - cx) / fx, (s.v - cy) / fy, 1.0);
      const double B = n.dot(d);
      if (std::abs(B) < 1e-6) {
        continue;
      }
      const double z_pred = n.dot(t) / B;
      if (z_pred <= 0.0) {
        continue;
      }
      const double r = z_pred - s.z;
      const double robust = std::abs(r) <= kHuber ? 1.0 : kHuber / std::abs(r);
      const double w = w_z * robust;
      Eigen::Matrix<double, 1, 6> J;
      J.block<1, 3>(0, 0) = (1.0 / B) * n.transpose();
      J.block<1, 3>(0, 3) = -(z_pred / B) * n.cross(d).transpose();
      H += w * J.transpose() * J;
      b += w * J.transpose() * r;
      sse_z += r * r;
      ++n_z;
    }

    // RMS residual in natural units: pixels for the corners, meters for depth.
    rms_px = n_px > 0 ? std::sqrt(sse_px / n_px) : 0.0;
    rms_z = n_z > 0 ? std::sqrt(sse_z / n_z) : 0.0;
    if (it == 0) {
      rms_px0 = rms_px;
      rms_z0 = rms_z;
    }

    H.diagonal().array() += 1e-9;  // numerical regularization
    const Eigen::Matrix<double, 6, 1> dx = H.ldlt().solve(-b);
    if (!dx.allFinite()) {
      break;
    }
    iters = it + 1;
    LogD("[refine] it={} uv_rms={:.3f}px z_rms={:.4f}m dx={:.3e}", it, rms_px,
         rms_z, dx.norm());
    T = Sophus::SE3d::exp(dx) * T;
    if (dx.norm() < 1e-7) {
      break;
    }
  }

  LogI(
      "[refine] {} corners {} depth | uv {:.3f}->{:.3f}px z {:.4f}->{:.4f}m in "
      "{} iters",
      n_px, n_z, rms_px0, rms_px, rms_z0, rms_z, iters);
  return T;
}

bool TrackCornersWithOpticalFlow(const std::vector<cv::Mat>& prev_pyramid,
                                 const std::vector<cv::Mat>& curr_pyramid,
                                 const std::vector<cv::Point2f>& prev_corners,
                                 double fb_threshold,
                                 std::vector<cv::Point2f>& out_corners) {
  if (prev_pyramid.empty() || curr_pyramid.empty() || prev_corners.empty()) {
    return false;
  }

  const cv::Size win(21, 21);
  std::vector<cv::Point2f> forward, backward;
  std::vector<uchar> status_fwd, status_bwd;
  std::vector<float> err_fwd, err_bwd;

  cv::calcOpticalFlowPyrLK(prev_pyramid, curr_pyramid, prev_corners, forward,
                           status_fwd, err_fwd, win);
  cv::calcOpticalFlowPyrLK(curr_pyramid, prev_pyramid, forward, backward,
                           status_bwd, err_bwd, win);

  for (size_t i = 0; i < prev_corners.size(); ++i) {
    if (!status_fwd[i] || !status_bwd[i]) {
      return false;
    }
    if (cv::norm(prev_corners[i] - backward[i]) > fb_threshold) {
      return false;
    }
  }
  out_corners = std::move(forward);
  return true;
}

Tag MakeTag(int id, const TrackerConfig& config) {
  std::unique_ptr<PoseFilter> filter;
  if (config.filter_enabled) {
    filter = std::make_unique<OneEuroPoseFilter>(
        config.pos_min_cutoff, config.pos_beta, config.rot_min_cutoff,
        config.rot_beta, config.d_cutoff, config.filter_translation,
        config.filter_rotation);
  }
  return Tag(id, std::move(filter));
}
}  // namespace

Tracker::Tracker()
    : detector_(
          cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11),
          MakeDetectorParams()) {}

Tracker::~Tracker() {
  Stop();
}

bool Tracker::Init(const TrackerConfig& config) {
  config_ = config;
  LogI("tracker init success");
  return true;
}

void Tracker::SubmitFrame(FrameBuffer frame_buffer) {
  {
    std::lock_guard<std::mutex> lock(frame_buffer_lock);
    frame_buffer_queue_.push(std::move(frame_buffer));
  }
  frame_cv_.notify_one();
}

void Tracker::Start() {
  if (running_.exchange(true)) {
    return;
  }
  thread_ = std::thread(&Tracker::Run, this);
}

void Tracker::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  frame_cv_.notify_all();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Tracker::Run() {
  while (running_) {
    FrameBuffer frame_buffer;
    {
      std::unique_lock<std::mutex> lock(frame_buffer_lock);
      frame_cv_.wait(
          lock, [this] { return !running_ || !frame_buffer_queue_.empty(); });
      if (!running_ && frame_buffer_queue_.empty()) {
        return;
      }
      frame_buffer = std::move(frame_buffer_queue_.front());
      frame_buffer_queue_.pop();
    }
    ProcessFrame(std::move(frame_buffer));
  }
}

void Tracker::ProcessOnce() {
  FrameBuffer frame_buffer;
  {
    std::lock_guard<std::mutex> lock(frame_buffer_lock);
    if (frame_buffer_queue_.empty()) {
      return;
    }
    frame_buffer = std::move(frame_buffer_queue_.front());
    frame_buffer_queue_.pop();
  }
  ProcessFrame(std::move(frame_buffer));
}

void Tracker::ProcessFrame(FrameBuffer frame_buffer) {
  Frame frame(std::move(frame_buffer), config_.target_width);

  std::vector<int> ids;
  std::vector<std::vector<cv::Point2f>> corners, rejected;
  detector_.detectMarkers(frame.GetGray(), corners, ids, rejected);

  if (config_.flow_enabled && !prev_pyramid_.empty()) {
    std::unordered_set<int> detected(ids.begin(), ids.end());
    for (const auto& [id, prev] : prev_corners_) {
      if (detected.count(id)) {
        continue;
      }
      std::vector<cv::Point2f> tracked;
      if (TrackCornersWithOpticalFlow(prev_pyramid_, frame.GetPyramid(), prev,
                                      config_.flow_fb_threshold_px, tracked)) {
        ids.push_back(id);
        corners.push_back(std::move(tracked));
      }
    }
  }

  const cv::Matx33d K(frame.GetFx(), 0, frame.GetCx(),  //
                      0, frame.GetFy(), frame.GetCy(),  //
                      0, 0, 1);
  const cv::Vec<double, 5> dist(0, 0, 0, 0, 0);

  const float kTagSize = config_.tag_size_m;
  const std::vector<cv::Point3f> corners_3d = {
      {-kTagSize / 2, kTagSize / 2, 0},
      {kTagSize / 2, kTagSize / 2, 0},
      {kTagSize / 2, -kTagSize / 2, 0},
      {-kTagSize / 2, -kTagSize / 2, 0}};

  const Sophus::SE3d& T_w_c = frame.GetPose();
  Sophus::SE3d T_w_t;

  auto result = std::make_shared<TrackResult>();
  result->t_ns = frame.GetTimestampNs();
  result->T_w_c = ToPoseArray(T_w_c);
  result->intrinsics = {
      static_cast<float>(frame.GetFx()), static_cast<float>(frame.GetFy()),
      static_cast<float>(frame.GetCx()), static_cast<float>(frame.GetCy())};

  const cv::Mat& gray = frame.GetGray();
  result->gray.width = gray.cols;
  result->gray.height = gray.rows;
  result->gray.data.assign(gray.data, gray.data + gray.total());

  if (frame.HasDepth()) {
    const cv::Mat& depth = frame.GetDepth();
    result->depth.width = depth.cols;
    result->depth.height = depth.rows;
    const auto* begin = depth.ptr<float>(0);
    result->depth.data.assign(begin, begin + depth.total());
  }

  result->tags.reserve(tags_.size());

  for (size_t i = 0; i < ids.size(); ++i) {
    std::vector<cv::Mat> rvecs, tvecs;
    std::vector<double> reproj_errors;
    cv::solvePnPGeneric(corners_3d, corners[i], K, dist, rvecs, tvecs, false,
                        cv::SOLVEPNP_IPPE_SQUARE, cv::noArray(), cv::noArray(),
                        reproj_errors);
    if (rvecs.empty()) {
      continue;
    }

    auto tag_it = tags_.find(ids[i]);
    if (tag_it == tags_.end()) {
      tag_it = tags_.emplace(ids[i], MakeTag(ids[i], config_)).first;
    }
    Tag& tag = tag_it->second;

    const std::optional<Sophus::SE3d> prev = tag.GetTwt();

    bool best_set = false;
    double best_score = 0.0;
    Sophus::SE3d best_T_c_t;
    for (size_t s = 0; s < rvecs.size(); ++s) {
      const Sophus::SE3d T_c_t = CamFromTag(rvecs[s], tvecs[s]);
      const Sophus::SE3d cand = T_w_c * T_c_t;

      double score = std::numeric_limits<double>::infinity();
      if (frame.HasDepth()) {
        score = DepthScore(T_c_t, corners_3d, corners[i], frame);
      }
      if (!std::isfinite(score)) {
        score = prev ? (prev->inverse() * cand).log().norm() : reproj_errors[s];
      }
      if (!best_set || score < best_score) {
        best_score = score;
        best_T_c_t = T_c_t;
        best_set = true;
      }
    }

    if (config_.depth_refine_enabled && frame.HasDepth()) {
      std::vector<DepthObservation> depth_obs =
          SampleQuadDepth(frame, corners[i], config_.max_depth_samples);
      best_T_c_t =
          RefinePose(best_T_c_t, frame, corners_3d, corners[i], depth_obs,
                     config_.refine_sigma_px, config_.refine_sigma_z,
                     config_.refine_huber_m, config_.refine_max_iters);
    }
    T_w_t = T_w_c * best_T_c_t;

    tag.AddPose(frame.GetTimestampNs(), T_w_t);
  }

  if (config_.flow_enabled) {
    prev_corners_.clear();
    for (size_t i = 0; i < ids.size(); ++i) {
      prev_corners_[ids[i]] = corners[i];
    }
    prev_pyramid_ = frame.GetPyramid();
  }

  const int64_t now_ns = frame.GetTimestampNs();
  for (auto it = tags_.begin(); it != tags_.end();) {
    const Tag& tag = it->second;
    if (now_ns - tag.GetTimestampNs() > config_.tag_discard_time_threshold) {
      it = tags_.erase(it);
      continue;
    }
    if (const std::optional<Sophus::SE3d> T_w_t = tag.GetTwt()) {
      result->tags.push_back({it->first, ToPoseArray(*T_w_t)});
    }
    ++it;
  }

  {
    std::lock_guard<std::mutex> lock(result_mutex_);
    latest_result_ = std::move(result);
  }

  LogI("frame t_ns: {}  tags: {}", frame.GetTimestampNs(), ids.size());
}

std::shared_ptr<const TrackResult> Tracker::GetLatestResult() const {
  std::lock_guard<std::mutex> lock(result_mutex_);
  return latest_result_;
}

}  // namespace tagar
