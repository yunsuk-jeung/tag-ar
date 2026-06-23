#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <sophus/se3.hpp>

#include "tagar/pose_filter.hpp"

namespace tagar {

// Scalar exponential low-pass: y = alpha * x + (1 - alpha) * y_prev.
class LowPassFilter {
 public:
  double Filter(double x, double alpha) {
    if (!initialized_) {
      y_ = x;
      initialized_ = true;
    } else {
      y_ = alpha * x + (1.0 - alpha) * y_;
    }
    return y_;
  }

 private:
  double y_ = 0.0;
  bool initialized_ = false;
};

// One Euro filter (Casiez, Roussel & Vogel, 2012): a speed-adaptive low-pass.
class OneEuroFilter {
 public:
  OneEuroFilter(double min_cutoff = 1.0, double beta = 0.0,
                double d_cutoff = 1.0)
      : min_cutoff_(min_cutoff), beta_(beta), d_cutoff_(d_cutoff) {}

  double Filter(double x, double dt) {
    if (dt <= 0.0) {
      dt = 1e-3;
    }
    const double dx = has_prev_ ? (x - x_prev_) / dt : 0.0;
    const double edx = dx_lp_.Filter(dx, Alpha(d_cutoff_, dt));
    const double cutoff = min_cutoff_ + beta_ * std::abs(edx);
    const double y = x_lp_.Filter(x, Alpha(cutoff, dt));
    x_prev_ = x;
    has_prev_ = true;
    return y;
  }

 private:
  static double Alpha(double cutoff, double dt) {
    constexpr double kTwoPi = 6.283185307179586;
    const double tau = 1.0 / (kTwoPi * cutoff);
    return 1.0 / (1.0 + tau / dt);
  }

  double min_cutoff_;
  double beta_;
  double d_cutoff_;
  LowPassFilter x_lp_;
  LowPassFilter dx_lp_;
  double x_prev_ = 0.0;
  bool has_prev_ = false;
};

class OneEuroPoseFilter : public PoseFilter {
 public:
  OneEuroPoseFilter() = default;
  OneEuroPoseFilter(double pos_min_cutoff, double pos_beta,
                    double rot_min_cutoff, double rot_beta, double d_cutoff,
                    bool filter_translation = true, bool filter_rotation = true)
      : t_{OneEuroFilter(pos_min_cutoff, pos_beta, d_cutoff),
           OneEuroFilter(pos_min_cutoff, pos_beta, d_cutoff),
           OneEuroFilter(pos_min_cutoff, pos_beta, d_cutoff)},
        q_{OneEuroFilter(rot_min_cutoff, rot_beta, d_cutoff),
           OneEuroFilter(rot_min_cutoff, rot_beta, d_cutoff),
           OneEuroFilter(rot_min_cutoff, rot_beta, d_cutoff),
           OneEuroFilter(rot_min_cutoff, rot_beta, d_cutoff)},
        filter_t_(filter_translation),
        filter_r_(filter_rotation) {}

  Sophus::SE3d Filter(const Sophus::SE3d& pose, int64_t t_ns) override {
    const bool has_prev = last_t_ns_ >= 0;
    const double dt =
        has_prev ? static_cast<double>(t_ns - last_t_ns_) * 1e-9 : 1.0 / 60.0;
    last_t_ns_ = t_ns;

    const Eigen::Vector3d t = pose.translation();
    const Eigen::Vector3d tf =
        filter_t_
            ? Eigen::Vector3d(t_[0].Filter(t.x(), dt), t_[1].Filter(t.y(), dt),
                              t_[2].Filter(t.z(), dt))
            : t;

    Eigen::Quaterniond q = pose.unit_quaternion();
    if (has_prev && q.dot(q_prev_) < 0.0) {
      q.coeffs() = -q.coeffs();  // keep the shortest-arc representation
    }
    Eigen::Quaterniond qf = q;
    if (filter_r_) {
      qf = Eigen::Quaterniond(q_[0].Filter(q.w(), dt), q_[1].Filter(q.x(), dt),
                              q_[2].Filter(q.y(), dt), q_[3].Filter(q.z(), dt));
      qf.normalize();
    }

    q_prev_ = qf;
    last_output_ = Sophus::SE3d(qf, tf);
    return last_output_;
  }

 private:
  std::array<OneEuroFilter, 3> t_;
  std::array<OneEuroFilter, 4> q_;
  Eigen::Quaterniond q_prev_ = Eigen::Quaterniond::Identity();
  Sophus::SE3d last_output_;
  int64_t last_t_ns_ = -1;
  bool filter_t_ = true;
  bool filter_r_ = true;
};

}  // namespace tagar
