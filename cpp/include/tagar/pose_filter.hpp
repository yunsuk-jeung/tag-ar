#pragma once

#include <cstdint>
#include <optional>

#include <sophus/se3.hpp>

namespace tagar {

class PoseFilter {
 public:
  virtual ~PoseFilter() = default;

  virtual Sophus::SE3d Filter(const Sophus::SE3d& pose, int64_t t_ns) = 0;
};

}  // namespace tagar
