#pragma once

#include <cstdint>

#include <sophus/se3.hpp>
#include <opencv2/core.hpp>

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

 protected:
  int64_t t_ns_;
  cv::Mat gray_;
  Sophus::SE3d T_w_c_;
  double fx_, fy_, cx_, cy_;
};
}  // namespace tagar
