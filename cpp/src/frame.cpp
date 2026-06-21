#include "tagar/frame.hpp"

#include <Eigen/Core>
#include <opencv2/imgproc.hpp>

namespace tagar {

Frame::Frame(FrameBuffer frame_buffer) {
  t_ns_ = frame_buffer.t_ns;
  fx_ = frame_buffer.intrinsics[0];
  fy_ = frame_buffer.intrinsics[1];
  cx_ = frame_buffer.intrinsics[2];
  cy_ = frame_buffer.intrinsics[3];

  ImageBuffer& image = frame_buffer.image_buffer;

  //todo: check when image is not RGB
  cv::Mat rgb(image.height, image.width, CV_8UC3, image.buffer.data());
  cv::cvtColor(rgb, gray_, cv::COLOR_RGB2GRAY);

  Eigen::Matrix4f pose =
      Eigen::Map<const Eigen::Matrix4f>(frame_buffer.pose.data());
  T_w_c_ = Sophus::SE3d::fitToSE3(pose.cast<double>());
}

Frame::~Frame() = default;

}  // namespace tagar
