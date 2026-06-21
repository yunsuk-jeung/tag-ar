#include "viewer/gl_camera.hpp"

#include <cmath>

namespace viz {
namespace {

const float kPi = std::acos(-1.0f);

float ToRad(float degree) {
  return degree * kPi / 180.0f;
}

// Maps world axes so the camera looks down +X with Z up.
const Eigen::Matrix3f kToZFront =
    (Eigen::Matrix3f() << 0, 0, 1, 1, 0, 0, 0, 1, 0).finished();

}  // namespace

void GLCamera::Init(int width, int height) {
  UpdateViewMatrix();
  OnResize(width, height);
}

void GLCamera::OnResize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  window_size_ << width, height;
  inverse_aspect_ = static_cast<float>(height) / static_cast<float>(width);
  UpdateProjMatrix();
}

void GLCamera::OnMouseDown(Button btn, double x, double y) {
  button_ = btn;
  last_mouse_ << x, y;
}

void GLCamera::OnMouseUp() {
  button_ = Button::kNone;
}

void GLCamera::OnMouseMove(double x, double y) {
  if (button_ == Button::kNone) {
    return;
  }

  Eigen::Vector2d abs_pos(x, y);
  Eigen::Vector2f relative_delta = AbsToRelative(abs_pos - last_mouse_);
  last_mouse_ = abs_pos;

  const float scale_factor_rotation = 1.5f;
  const float scale_factor_translation = 1.0f;

  switch (button_) {
    case Button::kLeft: {                                // rotation
      relative_delta *= 180.0f * scale_factor_rotation;  // to degrees
      latitude_ += relative_delta.y();
      latitude_ = (90.0f < latitude_) ? 90.0f : latitude_;
      latitude_ = (latitude_ < -90.0f) ? -90.0f : latitude_;

      longitude_ -= relative_delta.x();
      longitude_ = (360.0f < longitude_) ? (longitude_ - 360.0f) : longitude_;
      longitude_ = (longitude_ < 0.0f) ? (longitude_ + 360.0f) : longitude_;
      break;
    }
    case Button::kRight: {  // pan
      relative_delta *= distance_ * scale_factor_translation;
      Eigen::Matrix3f orientation =
          (Eigen::AngleAxisf(ToRad(longitude_), Eigen::Vector3f::UnitZ()) *
           Eigen::AngleAxisf(-ToRad(latitude_), Eigen::Vector3f::UnitY()))
              .toRotationMatrix();
      center_ += orientation *
                 Eigen::Vector3f(0.0f, -relative_delta.x(), relative_delta.y());
      break;
    }
    default:
      break;
  }
  UpdateViewMatrix();
}

void GLCamera::OnScroll(double yoffset) {
  if (yoffset > 0) {  // zoom in
    if (distance_ < 0.05f) {
      return;
    }
    distance_ *= 0.9f;
  } else {  // zoom out
    if ((far_ * 0.4f) < distance_) {
      return;
    }
    distance_ *= 1.1f;
  }
  UpdateViewMatrix();
}

Eigen::Vector2f GLCamera::AbsToRelative(const Eigen::Vector2d& delta) const {
  // Both components are divided by width on purpose (matches svis).
  const float w = static_cast<float>(window_size_.x());
  return Eigen::Vector2f(static_cast<float>(delta.x()) / w,
                         static_cast<float>(delta.y()) / w);
}

void GLCamera::UpdateViewMatrix() {
  Eigen::Matrix3f orientation =
      (Eigen::AngleAxisf(ToRad(longitude_), Eigen::Vector3f::UnitZ()) *
       Eigen::AngleAxisf(-ToRad(latitude_), Eigen::Vector3f::UnitY()))
          .toRotationMatrix();
  view_matrix_.topLeftCorner<3, 3>() = (orientation * kToZFront).transpose();
  view_matrix_.topRightCorner<3, 1>() =
      -view_matrix_.topLeftCorner<3, 3>() *
      (orientation * Eigen::Vector3f(distance_, 0, 0) + center_);
}

void GLCamera::UpdateProjMatrix() {
  const float tmp = std::tan(hfov_ / 360.0f * kPi);
  proj_matrix_ << (1.0f / tmp), 0, 0, 0, 0, (1.0f / (inverse_aspect_ * tmp)), 0,
      0, 0, 0, -(far_ + near_) / (far_ - near_),
      -(2 * far_ * near_) / (far_ - near_), 0, 0, -1, 0;
}

}  // namespace viz
