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

  Tag() = delete;
  Tag(int id, size_t buffer_size) : id_(id), buffer_size_(buffer_size) {}
  ~Tag() = default;

  int GetId() const { return id_; }

  void AddPose(int64_t t_ns, const Sophus::SE3d& T_w_t) {
    T_w_t_buffer_.push_back({t_ns, T_w_t});
    if (T_w_t_buffer_.size() > buffer_size_) {
      T_w_t_buffer_.pop_front();
    }
  }

  bool Empty() const { return T_w_t_buffer_.empty(); }
  size_t Count() const { return T_w_t_buffer_.size(); }

  std::optional<Sophus::SE3d> GetLatestPose() const {
    if (T_w_t_buffer_.empty()) {
      return std::nullopt;
    }
    return T_w_t_buffer_.back().T_w_t;
  }

  // Smoothed world pose over the buffer (simple mean); nullopt if empty.
  std::optional<Sophus::SE3d> GetFilteredPose() const;

 protected:
  int id_;
  size_t buffer_size_;
  std::deque<TimedPose> T_w_t_buffer_;
};
}  // namespace tagar
