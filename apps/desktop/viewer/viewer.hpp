#pragma once

#include <string>

#include <Eigen/Core>

#include "viewer/gl_camera.hpp"
#include "viewer/shader.hpp"

struct GLFWwindow;

namespace viz {

class MeshRenderer;

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

  void Draw(const MeshRenderer& mesh, const Eigen::Matrix4f& model);

  GLCamera& GetCamera() { return camera_; }

 private:
  void InstallCallbacks();

  GLFWwindow* window_ = nullptr;
  int width_ = 0;
  int height_ = 0;

  GLCamera camera_;
  Shader shader_;
};

}  // namespace viz
