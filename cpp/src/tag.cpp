#include "tagar/tag.hpp"

#include <numbers>

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace tagar {

void Tag::AddPose(int64_t t_ns, const Sophus::SE3d& T_w_t) {
  T_w_t_buffer_.push_back({t_ns, T_w_t});
  if (T_w_t_buffer_.size() > buffer_size_) {
    T_w_t_buffer_.pop_front();
  }

  if (!has_filtered_) {
    filtered_pose_ = T_w_t;
    has_filtered_ = true;
    reject_count_ = 0;
    return;
  }

  const double d_trans =
      (T_w_t.translation() - filtered_pose_.translation()).norm();
  const double d_rot =
      (filtered_pose_.so3().inverse() * T_w_t.so3()).log().norm();
  const double rot_gate_rad = filter_.rot_gate_deg * std::numbers::pi / 180.0;

  if (d_trans > filter_.trans_gate_m || d_rot > rot_gate_rad) {
    if (++reject_count_ >= filter_.max_rejects) {
      filtered_pose_ = T_w_t;
      reject_count_ = 0;
    }
    return;
  }
  reject_count_ = 0;

  const double a = filter_.alpha;
  const Eigen::Vector3d t =
      (1.0 - a) * filtered_pose_.translation() + a * T_w_t.translation();
  const Eigen::Quaterniond q =
      filtered_pose_.unit_quaternion().slerp(a, T_w_t.unit_quaternion());
  filtered_pose_ = Sophus::SE3d(q, t);
}

}  // namespace tagar
