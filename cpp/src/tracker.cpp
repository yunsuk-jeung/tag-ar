#include <algorithm>
#include <cerrno>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <opencv2/calib3d.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>

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
    return;  // already running
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
  Frame frame(std::move(frame_buffer));

  std::vector<int> ids;
  std::vector<std::vector<cv::Point2f>> corners, rejected;
  detector_.detectMarkers(frame.GetGray(), corners, ids, rejected);

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
        T_w_t = cand;
        best_set = true;
      }
    }

    tag.AddPose(frame.GetTimestampNs(), T_w_t);
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