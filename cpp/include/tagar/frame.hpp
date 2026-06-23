#pragma once

#include <cstdint>

#include <opencv2/core.hpp>
#include <sophus/se3.hpp>

#include "tagar/types.hpp"

namespace tagar {
class Frame {
 public:
  Frame() = delete;
  Frame(FrameBuffer frame_buffer);
  ~Frame();

  int64_t GetTimestampNs() const { return t_ns_; }
  const cv::Mat& GetGray() const { return gray_; }
  const Sophus::SE3d& GetPose() const { return T_w_c_; }
  double GetFx() const { return fx_; }
  double GetFy() const { return fy_; }
  double GetCx() const { return cx_; }
  double GetCy() const { return cy_; }

  bool HasDepth() const { return !depth_.empty(); }
  const cv::Mat& GetDepth() const { return depth_; }
  float DepthAt(float u_color, float v_color) const;

 protected:
  int64_t t_ns_;
  cv::Mat gray_;
  cv::Mat depth_;  // CV_32F, meters; empty if unavailable
  float depth_scale_x_ = 1.0f;
  float depth_scale_y_ = 1.0f;
  Sophus::SE3d T_w_c_;
  double fx_, fy_, cx_, cy_;
};
}  // namespace tagar
