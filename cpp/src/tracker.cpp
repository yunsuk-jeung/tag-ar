#include <mutex>
#include <utility>
#include <vector>

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

bool Tracker::Init() {
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

  constexpr float kTagSize = 0.08f;  // meters
  const std::vector<cv::Point3f> object_points = {
      {-kTagSize / 2, kTagSize / 2, 0},
      {kTagSize / 2, kTagSize / 2, 0},
      {kTagSize / 2, -kTagSize / 2, 0},
      {-kTagSize / 2, -kTagSize / 2, 0}};

  // Estimate each tag's 6DoF pose in the camera frame.
  for (size_t i = 0; i < ids.size(); ++i) {
    cv::Vec3d rvec, tvec;
    cv::solvePnP(object_points, corners[i], K, dist, rvec, tvec, false,
                 cv::SOLVEPNP_IPPE_SQUARE);
    LogI("tag {} pos(cam)=[{:.3f}, {:.3f}, {:.3f}] m  dist={:.3f} m", ids[i],
         tvec[0], tvec[1], tvec[2], cv::norm(tvec));
  }

  debug::VisualizeTag(frame, corners, ids);

  LogI("frame t_ns: {}  tags: {}", frame.GetTimestampNs(), ids.size());
}

void Tracker::Process() {}

}  // namespace tagar