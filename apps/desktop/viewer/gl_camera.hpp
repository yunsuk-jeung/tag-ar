#pragma once

#include <Eigen/Dense>

namespace viz {

class GLCamera {
 public:
  enum class Button { kNone = -1, kLeft = 0, kRight = 1, kMiddle = 2 };

  GLCamera() = default;

  void Init(int width, int height);
  void OnResize(int width, int height);

  void OnMouseDown(Button btn, double x, double y);
  void OnMouseUp();
  void OnMouseMove(double x, double y);
  void OnScroll(double yoffset);

  const Eigen::Matrix4f& GetViewMatrix() const { return view_matrix_; }
  const Eigen::Matrix4f& GetProjMatrix() const { return proj_matrix_; }

 private:
  Eigen::Vector2f AbsToRelative(const Eigen::Vector2d& delta) const;
  void UpdateViewMatrix();
  void UpdateProjMatrix();

  Eigen::Vector2i window_size_ = Eigen::Vector2i(1, 1);
  Eigen::Vector2d last_mouse_ = Eigen::Vector2d::Zero();
  Button button_ = Button::kNone;

  Eigen::Vector3f center_ = Eigen::Vector3f::Zero();
  float distance_ = 2.0f;
  float longitude_ = 45.0f;
  float latitude_ = 45.0f;

  float hfov_ = 60.0f;
  float inverse_aspect_ = 1.0f;  // height / width
  float near_ = 0.01f;
  float far_ = 5000.0f;

  Eigen::Matrix4f view_matrix_ = Eigen::Matrix4f::Identity();
  Eigen::Matrix4f proj_matrix_ = Eigen::Matrix4f::Identity();
};

}  // namespace viz
