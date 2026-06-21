#pragma once

#include "viewer/gl_loader.hpp"

namespace viz {

class RenderTarget {
 public:
  RenderTarget() = default;
  ~RenderTarget();

  RenderTarget(const RenderTarget&) = delete;
  RenderTarget& operator=(const RenderTarget&) = delete;

  bool Create(int width, int height);

  // Binds the FBO and sets the viewport to its size.
  void Bind() const;
  // Restores the default framebuffer and the given window viewport.
  static void Unbind(int window_width, int window_height);

  GLuint GetColorTexture() const { return color_; }
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

 private:
  GLuint fbo_ = 0;
  GLuint color_ = 0;
  GLuint depth_ = 0;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace viz
