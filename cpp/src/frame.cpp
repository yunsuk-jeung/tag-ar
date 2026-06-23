#include <cmath>

#include <Eigen/Core>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include "tagar/types.hpp"

#include "tagar/frame.hpp"

namespace tagar {

namespace {
constexpr int kFlowWin = 21;
constexpr int kFlowMaxLevel = 3;
}  // namespace

Frame::Frame(FrameBuffer frame_buffer, int target_width) {
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

  if (target_width > 0 && target_width < gray_.cols) {
    const double scale = static_cast<double>(target_width) / gray_.cols;
    const int h = static_cast<int>(std::lround(gray_.rows * scale));
    cv::resize(gray_, gray_, cv::Size(target_width, h), 0, 0, cv::INTER_AREA);
    fx_ *= scale;
    fy_ *= scale;
    cx_ *= scale;
    cy_ *= scale;
  }

  const DepthBuffer& depth = frame_buffer.depth_buffer;
  if (!depth.data.empty() && depth.width > 0 && depth.height > 0) {
    cv::Mat wrapped(depth.height, depth.width, CV_32F,
                    const_cast<float*>(depth.data.data()));
    depth_ = wrapped.clone();
    depth_scale_x_ = static_cast<float>(depth.width) / gray_.cols;
    depth_scale_y_ = static_cast<float>(depth.height) / gray_.rows;
  }

  Eigen::Matrix4f pose =
      Eigen::Map<const Eigen::Matrix4f>(frame_buffer.pose.data());
  T_w_c_ = Sophus::SE3d::fitToSE3(pose.cast<double>());
}

float Frame::DepthAt(float u_color, float v_color) const {
  if (depth_.empty()) {
    return -1.0f;
  }
  const int u = static_cast<int>(std::lround(u_color * depth_scale_x_));
  const int v = static_cast<int>(std::lround(v_color * depth_scale_y_));
  if (u < 0 || v < 0 || u >= depth_.cols || v >= depth_.rows) {
    return -1.0f;
  }
  const float z = depth_.at<float>(v, u);
  if (!std::isfinite(z) || z <= 0.0f) {
    return -1.0f;
  }
  return z;
}

const std::vector<cv::Mat>& Frame::GetPyramid() {
  if (pyramid_.empty() && !gray_.empty()) {
    cv::buildOpticalFlowPyramid(gray_, pyramid_, cv::Size(kFlowWin, kFlowWin),
                                kFlowMaxLevel);
  }
  return pyramid_;
}

Frame::~Frame() = default;

}  // namespace tagar
