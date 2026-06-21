#include "viewer/viewer.hpp"

#include <cstdio>

#include "viewer/gl_loader.hpp"
#include "viewer/mesh_renderer.hpp"

namespace viz {
namespace {

void GlfwErrorCallback(int error, const char* description) {
  std::fprintf(stderr, "glfw error %d: %s\n", error, description);
}

void KeyCallback(GLFWwindow* window, int key, int /*scancode*/, int action,
                 int /*mods*/) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
}

}  // namespace

Viewer::~Viewer() {
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

bool Viewer::Init(int width, int height, const std::string& title,
                  const std::string& shader_dir) {
  width_ = width;
  height_ = height;

  glfwSetErrorCallback(GlfwErrorCallback);
  if (!glfwInit()) {
    std::fprintf(stderr, "viewer: glfwInit failed\n");
    return false;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!window_) {
    std::fprintf(stderr, "Viewer: glfwCreateWindow failed\n");
    glfwTerminate();
    return false;
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);

  if (!shader_.Load(shader_dir + "/mesh.vert", shader_dir + "/mesh.frag")) {
    std::fprintf(stderr, "Viewer: failed to load shaders from %s\n",
                 shader_dir.c_str());
    return false;
  }

  int win_w = 0;
  int win_h = 0;
  glfwGetWindowSize(window_, &win_w, &win_h);
  camera_.Init(win_w, win_h);

  InstallCallbacks();

  glEnable(GL_DEPTH_TEST);

  std::printf("Viewer GL_VERSION: %s\n", glGetString(GL_VERSION));
  std::printf("Viewer GLSL: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
  return true;
}

void Viewer::InstallCallbacks() {
  glfwSetWindowUserPointer(window_, this);
  glfwSetKeyCallback(window_, KeyCallback);

  glfwSetWindowSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
    auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w));
    self->camera_.OnResize(width, height);
  });

  glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int button, int action,
                                         int /*mods*/) {
    auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w));
    double x = 0;
    double y = 0;
    glfwGetCursorPos(w, &x, &y);
    if (action == GLFW_PRESS) {
      self->camera_.OnMouseDown(static_cast<GLCamera::Button>(button), x, y);
    } else if (action == GLFW_RELEASE) {
      self->camera_.OnMouseUp();
    }
  });

  glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double x, double y) {
    auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w));
    self->camera_.OnMouseMove(x, y);
  });

  glfwSetScrollCallback(
      window_, [](GLFWwindow* w, double /*xoff*/, double yoff) {
        auto* self = static_cast<Viewer*>(glfwGetWindowUserPointer(w));
        self->camera_.OnScroll(yoff);
      });
}

bool Viewer::ShouldClose() const {
  return window_ == nullptr || glfwWindowShouldClose(window_);
}

void Viewer::PollEvents() {
  glfwPollEvents();
}

void Viewer::BeginFrame() {
  int fb_w = 0;
  int fb_h = 0;
  glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  glViewport(0, 0, fb_w, fb_h);

  glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Viewer::EndFrame() {
  glfwSwapBuffers(window_);
}

void Viewer::Draw(const MeshRenderer& mesh, const Eigen::Matrix4f& model,
                  GLuint texture) {
  const Eigen::Matrix4f mvp =
      camera_.GetProjMatrix() * camera_.GetViewMatrix() * model;
  mesh.Draw(shader_, mvp, texture);
}

}  // namespace viz
