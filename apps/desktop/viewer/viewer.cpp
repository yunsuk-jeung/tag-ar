#include <cstdio>

#include "viewer/viewer.hpp"
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

Eigen::Matrix4f ProjectionFromIntrinsics(float fx, float fy, float cx, float cy,
                                         float w, float h, float near,
                                         float far) {
  Eigen::Matrix4f p = Eigen::Matrix4f::Zero();
  p(0, 0) = 2.0f * fx / w;
  p(0, 2) = (2.0f * cx - w) / w;
  p(1, 1) = -2.0f * fy / h;
  p(1, 2) = (h - 2.0f * cy) / h;
  p(2, 2) = (far + near) / (far - near);
  p(2, 3) = -2.0f * far * near / (far - near);
  p(3, 2) = 1.0f;
  return p;
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

#if !defined(__APPLE__)
  // macOS gets the GL functions straight from OpenGL.framework, but on
  // Linux/Windows the function pointers must be resolved by GLEW *after* a
  // context is current and *before* any GL call. glewExperimental is required
  // for core-profile contexts, otherwise some entry points stay NULL.
  glewExperimental = GL_TRUE;
  if (GLenum err = glewInit(); err != GLEW_OK) {
    std::fprintf(stderr, "Viewer: glewInit failed: %s\n",
                 glewGetErrorString(err));
    glfwTerminate();
    return false;
  }
  // glewInit can leave a benign GL_INVALID_ENUM on core profiles; clear it.
  glGetError();
#endif

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

  inset_rt_.Create(640, 480);
  inset_quad_.Upload(MakeQuad({1.0f, 1.0f, 1.0f}));

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

void Viewer::DrawReprojectionInset(
    const uint8_t* gray, int gray_w, int gray_h,
    const std::array<float, 4>& intrinsics, const Eigen::Matrix4f& T_w_c,
    const std::vector<std::pair<Eigen::Matrix4f, GLuint>>& tags) {
  if (gray == nullptr || gray_w <= 0 || gray_h <= 0) {
    return;
  }

  inset_gray_.UploadGray(gray, gray_w, gray_h);

  const Eigen::Matrix4f proj = ProjectionFromIntrinsics(
      intrinsics[0], intrinsics[1], intrinsics[2], intrinsics[3],
      static_cast<float>(gray_w), static_cast<float>(gray_h), 0.01f, 100.0f);
  const Eigen::Matrix4f view = T_w_c.inverse();

  inset_rt_.Bind();
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glDisable(GL_DEPTH_TEST);
  inset_quad_.Draw(shader_, Eigen::Matrix4f::Identity(), inset_gray_.GetId());

  glEnable(GL_DEPTH_TEST);
  for (const auto& [model, texture] : tags) {
    inset_quad_.Draw(shader_, proj * view * model, texture);
  }

  int fb_w = 0;
  int fb_h = 0;
  glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  RenderTarget::Unbind(fb_w, fb_h);

  const float img_aspect =
      static_cast<float>(gray_w) / static_cast<float>(gray_h);
  const float win_aspect = static_cast<float>(fb_w) / static_cast<float>(fb_h);
  const float half_h = 0.30f;
  const float half_w = half_h * img_aspect / win_aspect;
  const float margin = 0.03f;

  Eigen::Matrix4f place = Eigen::Matrix4f::Identity();
  place(0, 0) = half_w;
  place(1, 1) = -half_h;
  place(0, 3) = 1.0f - half_w - margin;
  place(1, 3) = -1.0f + half_h + margin;

  glDisable(GL_DEPTH_TEST);
  inset_quad_.Draw(shader_, place, inset_rt_.GetColorTexture());
  glEnable(GL_DEPTH_TEST);
}

}  // namespace viz
