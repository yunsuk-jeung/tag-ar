#include <Eigen/Core>
#include <Eigen/Geometry>

#include "tagar/tag.hpp"

namespace tagar {

std::optional<Sophus::SE3d> Tag::GetFilteredPose() const {
  if (T_w_t_buffer_.empty()) {
    return std::nullopt;
  }

  return std::nullopt;
}

}  // namespace tagar
