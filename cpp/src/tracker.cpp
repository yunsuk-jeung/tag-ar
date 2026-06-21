#include <cerrno>
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
          cv::aruco::DetectorParameters()) {}

Tracker::~Tracker() {}

bool Tracker::Init(const TrackerConfig& config) {
  config_ = config;
  LogI("tracker init success");
  return true;
}

void Tracker::SubmitFrame(FrameBuffer frame_buffer) {
  std::lock_guard<std::mutex> lock(frame_buffer_lock);
  frame_buffer_queue_.push(std::move(frame_buffer));
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

  Frame frame(std::move(frame_buffer));

  std::vector<int> ids;
  std::vector<std::vector<cv::Point2f>> corners, rejected;
  detector_.detectMarkers(frame.GetGray(), corners, ids, rejected);

  // Camera intrinsics from the frame; assume a pinhole model (no distortion).
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

  for (size_t i = 0; i < ids.size(); ++i) {
    cv::Vec3d rvec, tvec;
    cv::solvePnP(object_points, corners[i], K, dist, rvec, tvec, false,
                 cv::SOLVEPNP_IPPE_SQUARE);

    cv::Matx33d R;
    cv::Rodrigues(rvec, R);
    Eigen::Matrix4d cam_from_tag = Eigen::Matrix4d::Identity();
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        cam_from_tag(r, c) = R(r, c);
      }
      cam_from_tag(r, 3) = tvec[r];
    }

    Sophus::SE3d T_c_t = Sophus::SE3d::fitToSE3(cam_from_tag);

    T_w_t = T_w_c * T_c_t;

    const Eigen::Vector3d p = T_w_t.translation();
    LogI("tag {} pos(world)=[{:.3f}, {:.3f}, {:.3f}] m", ids[i], p.x(), p.y(),
         p.z());

    auto [it, _] =
        tags_.try_emplace(ids[i], ids[i], config_.tag_pose_buffer_size);
    Tag& tag = it->second;
    tag.AddPose(frame.GetTimestampNs(), T_w_t);
  }

  LogI("frame t_ns: {}  tags: {}", frame.GetTimestampNs(), ids.size());
}

void Tracker::Process() {}

}  // namespace tagar