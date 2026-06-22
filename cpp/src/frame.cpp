#include <Eigen/Core>
#include <opencv2/imgproc.hpp>

#include "tagar/frame.hpp"

namespace tagar {

Frame::Frame(FrameBuffer frame_buffer) {
  t_ns_ = frame_buffer.t_ns;
  fx_ = frame_buffer.intrinsics[0];
  fy_ = frame_buffer.intrinsics[1];
  cx_ = frame_buffer.intrinsics[2];
  cy_ = frame_buffer.intrinsics[3];

  ImageBuffer& image = frame_buffer.image_buffer;

  switch (image.format) {
    case ColorFormat::kGray: {
      cv::Mat gray(image.height, image.width, CV_8UC1, image.buffer.data());
      gray_ = gray.clone();
      break;
    }
    case ColorFormat::kNV12: {
      cv::Mat yuv(image.height * 3 / 2, image.width, CV_8UC1,
                  image.buffer.data());
      cv::cvtColor(yuv, gray_, cv::COLOR_YUV2GRAY_NV12);
      break;
    }
    case ColorFormat::kRGB:
    default: {
      cv::Mat rgb(image.height, image.width, CV_8UC3, image.buffer.data());
      cv::cvtColor(rgb, gray_, cv::COLOR_RGB2GRAY);
      break;
    }
  }

  Eigen::Matrix4f pose =
      Eigen::Map<const Eigen::Matrix4f>(frame_buffer.pose.data());
  T_w_c_ = Sophus::SE3d::fitToSE3(pose.cast<double>());
}

Frame::~Frame() = default;

}  // namespace tagar
