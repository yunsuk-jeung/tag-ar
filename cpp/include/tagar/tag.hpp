#pragma once

#include <sophus/se3.hpp>

namespace tagar {
class Tag {
 public:
  Tag() = delete;
  Tag(int id) : id_(id) {}
  ~Tag() = default;

  int GetId() const { return id_; }
  Sophus::SE3d GetTwc() const { return T_w_c_; }

 protected:
  int id_;
  Sophus::SE3d T_w_c_;
};
}  // namespace tagar