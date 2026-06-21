#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

#include <sophus/se3.hpp>

namespace tagar {

class Tag {
 public:
  struct TimedPose {
    int64_t t_ns;
    Sophus::SE3d T_w_t;
  };

  // Outlier-rejecting low-pass filter parameters.
  struct FilterParams {
    double alpha = 0.15;         // EMA weight on the new measurement
    double trans_gate_m = 0.05;  // reject translation jumps beyond this
    double rot_gate_deg = 60.0;  // reject rotation jumps beyond this
    int max_rejects = 5;         // re-acquire after this many rejects
  };

  Tag() = delete;
  Tag(int id, size_t buffer_size, FilterParams filter)
      : id_(id), buffer_size_(buffer_size), filter_(filter) {}
  ~Tag() = default;

  int GetId() const { return id_; }

  void AddPose(int64_t t_ns, const Sophus::SE3d& T_w_t);

  bool Empty() const { return T_w_t_buffer_.empty(); }
  size_t Count() const { return T_w_t_buffer_.size(); }

  std::optional<Sophus::SE3d> GetLatestPose() const {
    if (T_w_t_buffer_.empty()) {
      return std::nullopt;
    }
    return T_w_t_buffer_.back().T_w_t;
  }

  std::optional<Sophus::SE3d> GetFilteredPose() const {
    if (!has_filtered_) {
      return std::nullopt;
    }
    return filtered_pose_;
  }

 protected:
  int id_;
  size_t buffer_size_;
  std::deque<TimedPose> T_w_t_buffer_;

  FilterParams filter_;
  bool has_filtered_ = false;
  Sophus::SE3d filtered_pose_;
  int reject_count_ = 0;
};
}  // namespace tagar
