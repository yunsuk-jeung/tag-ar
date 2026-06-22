#include <algorithm>
#include <cerrno>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>

#include "tagar/tracker.hpp"
#include "tagar/frame.hpp"
#include "tagar/logger.hpp"

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
}  // namespace

namespace debug {
void VisualizeTag(Frame& frame, std::vector<std::vector<cv::Point2f>> corners,
                  std::vector<int> ids) {
  cv::Mat vis;
  cv::cvtColor(frame.GetGray(), vis, cv::COLOR_GRAY2BGR);
  cv::aruco::drawDetectedMarkers(vis, corners, ids);
  cv::imshow("tags", vis);
  cv::waitKey();
}
}  // namespace debug

Tracker::Tracker()
    : detector_(
          cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11),
          MakeDetectorParams()) {}

Tracker::~Tracker() { Stop(); }

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
      frame_cv_.wait(lock, [this] {
        return !running_ || !frame_buffer_queue_.empty();
      });
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
  const std::vector<cv::Point3f> object_points = {
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
  result->tags.reserve(ids.size());

  //TODO apply depth
  for (size_t i = 0; i < ids.size(); ++i) {
    std::vector<cv::Mat> rvecs, tvecs;
    std::vector<double> reproj_errors;
    cv::solvePnPGeneric(object_points, corners[i], K, dist, rvecs, tvecs, false,
                        cv::SOLVEPNP_IPPE_SQUARE, cv::noArray(), cv::noArray(),
                        reproj_errors);
    if (rvecs.empty()) {
      continue;
    }

    std::optional<Sophus::SE3d> prev;
    if (auto it = tags_.find(ids[i]); it != tags_.end()) {
      prev = it->second.GetLatestPose();
    }

    bool best_set = false;
    double best_score = 0.0;
    for (size_t s = 0; s < rvecs.size(); ++s) {
      const Sophus::SE3d cand = T_w_c * CamFromTag(rvecs[s], tvecs[s]);
      const double score =
          prev ? (prev->inverse() * cand).log().norm() : reproj_errors[s];
      if (!best_set || score < best_score) {
        best_score = score;
        T_w_t = cand;
        best_set = true;
      }
    }

    const Eigen::Vector3d p = T_w_t.translation();

    auto [it, _] =
        tags_.try_emplace(ids[i], ids[i], config_.tag_pose_buffer_size);
    Tag& tag = it->second;
    tag.AddPose(frame.GetTimestampNs(), T_w_t);

    result->tags.push_back({ids[i], ToPoseArray(T_w_t)});
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