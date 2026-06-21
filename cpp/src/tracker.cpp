#include <algorithm>
#include <cerrno>
#include <memory>
#include <mutex>
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
constexpr int kBoardId = 0;

std::array<float, 16> ToPoseArray(const Sophus::SE3d& T) {
  Eigen::Matrix4f m = T.matrix().cast<float>();
  std::array<float, 16> out{};
  std::copy(m.data(), m.data() + 16, out.begin());
  return out;
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
          cv::aruco::DetectorParameters()) {
  cv::aruco::DetectorParameters params = detector_.getDetectorParameters();
  params.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;
  detector_.setDetectorParameters(params);
}

Tracker::~Tracker() {
  Stop();
}

bool Tracker::Init(const TrackerConfig& config) {
  config_ = config;

  const auto dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
  board_.emplace(cv::Size(config_.board_markers_x, config_.board_markers_y),
                 config_.marker_length_m, config_.marker_separation_m,
                 dictionary);

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

  const Sophus::SE3d& T_w_c = frame.GetPose();

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

  cv::Mat board_obj_pts;
  cv::Mat board_img_pts;
  if (!ids.empty()) {
    board_->matchImagePoints(corners, ids, board_obj_pts, board_img_pts);
  }

  if (!board_obj_pts.empty() && board_obj_pts.total() >= 4) {
    cv::Vec3d rvec;
    cv::Vec3d tvec;
    cv::solvePnP(board_obj_pts, board_img_pts, K, dist, rvec, tvec);

    cv::Matx33d R;
    cv::Rodrigues(rvec, R);
    Eigen::Matrix4d cam_from_board = Eigen::Matrix4d::Identity();
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        cam_from_board(r, c) = R(r, c);
      }
      cam_from_board(r, 3) = tvec[r];
    }

    const Sophus::SE3d T_c_b = Sophus::SE3d::fitToSE3(cam_from_board);

    const cv::Point3f rb = board_->getRightBottomCorner();
    const Sophus::SE3d T_b_center(
        Sophus::SO3d(), Eigen::Vector3d(rb.x * 0.5, rb.y * 0.5, rb.z * 0.5));
    const Sophus::SE3d T_w_b = T_w_c * T_c_b * T_b_center;

    const Tag::FilterParams filter{
        config_.filter_alpha, config_.filter_trans_gate_m,
        config_.filter_rot_gate_deg, config_.filter_max_rejects};
    auto [it, _] = tags_.try_emplace(kBoardId, kBoardId,
                                     config_.tag_pose_buffer_size, filter);
    Tag& board = it->second;
    board.AddPose(frame.GetTimestampNs(), T_w_b);

    const Sophus::SE3d filtered = board.GetFilteredPose().value_or(T_w_b);
    result->board_pose = ToPoseArray(filtered);

    const Eigen::Vector3d p = filtered.translation();
    LogI("board pos(world)=[{:.3f}, {:.3f}, {:.3f}] m ({} markers)", p.x(),
         p.y(), p.z(), ids.size());
  }

  {
    std::lock_guard<std::mutex> lock(result_mutex_);
    latest_result_ = std::move(result);
  }

  LogI("frame t_ns: {}  markers: {}", frame.GetTimestampNs(), ids.size());
}

std::shared_ptr<const TrackResult> Tracker::GetLatestResult() const {
  std::lock_guard<std::mutex> lock(result_mutex_);
  return latest_result_;
}

}  // namespace tagar