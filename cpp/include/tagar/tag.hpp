#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include <sophus/se3.hpp>

#include "tagar/pose_filter.hpp"

namespace tagar {

class Tag {
 public:
  Tag() = delete;
  Tag(int id, std::unique_ptr<PoseFilter> filter)
      : id_(id), t_ns_(-1), filter_(std::move(filter)) {}
  ~Tag() = default;

  Tag(Tag&&) = default;
  Tag& operator=(Tag&&) = default;

  int GetId() const { return id_; }

  const Sophus::SE3d& AddPose(int64_t t_ns, const Sophus::SE3d& T_w_t);

  int64_t GetTns() const { return t_ns_; }

  // Latest published world pose; nullopt until the first AddPose. Validity is
  // tracked by t_ns_ (-1 before any pose), so no separate flag is needed.
  std::optional<Sophus::SE3d> GetTwt() const {
    if (t_ns_ < 0) {
      return std::nullopt;
    }
    return T_w_t_;
  }

 protected:
  int id_;
  std::unique_ptr<PoseFilter> filter_;
  int64_t t_ns_;
  Sophus::SE3d T_w_t_;
};

}  // namespace tagar
