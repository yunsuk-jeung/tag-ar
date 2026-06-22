#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "viewer/gl_camera.hpp"
#include "viewer/mesh_renderer.hpp"
#include "viewer/render_target.hpp"
#include "viewer/shader.hpp"
#include "viewer/texture.hpp"

struct GLFWwindow;

namespace viz {

class Viewer {
 public:
  Viewer() = default;
  ~Viewer();

  Viewer(const Viewer&) = delete;
  Viewer& operator=(const Viewer&) = delete;

  bool Init(int width, int height, const std::string& title,
            const std::string& shader_dir);

  bool ShouldClose() const;

  void PollEvents();

  void BeginFrame();
  void EndFrame();

  void Draw(const MeshRenderer& mesh, const Eigen::Matrix4f& model,
            GLuint texture = 0);

  void DrawReprojectionInset(
      const uint8_t* gray, int gray_w, int gray_h,
      const std::array<float, 4>& intrinsics, const Eigen::Matrix4f& T_w_c,
      const MeshRenderer& mesh,
      const std::vector<std::pair<Eigen::Matrix4f, GLuint>>& draws);

  GLCamera& GetCamera() { return camera_; }

 private:
  void InstallCallbacks();

  GLFWwindow* window_ = nullptr;
  int width_ = 0;
  int height_ = 0;

  GLCamera camera_;
  Shader shader_;

  RenderTarget inset_rt_;
  Texture inset_gray_;
  MeshRenderer inset_quad_;
};

}  // namespace viz
