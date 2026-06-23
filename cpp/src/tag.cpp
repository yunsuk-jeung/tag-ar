#include "tagar/tag.hpp"

namespace tagar {

const Sophus::SE3d& Tag::AddPose(int64_t t_ns, const Sophus::SE3d& T_w_t) {
  t_ns_ = t_ns;
  T_w_t_ = filter_ ? filter_->Filter(T_w_t, t_ns) : T_w_t;
  return T_w_t_;
}

}  // namespace tagar
