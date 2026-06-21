#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "viewer/texture.hpp"

namespace viz {

Texture::~Texture() {
  if (id_ != 0) {
    glDeleteTextures(1, &id_);
  }
}

Texture::Texture(Texture&& other) noexcept : id_(other.id_) {
  other.id_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
  if (this != &other) {
    if (id_ != 0) {
      glDeleteTextures(1, &id_);
    }
    id_ = other.id_;
    other.id_ = 0;
  }
  return *this;
}

bool Texture::LoadFromFile(const std::string& path) {
  cv::Mat bgr = cv::imread(path, cv::IMREAD_COLOR);
  if (bgr.empty()) {
    return false;
  }
  cv::Mat rgb;
  cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
  if (!rgb.isContinuous()) {
    rgb = rgb.clone();
  }

  if (id_ == 0) {
    glGenTextures(1, &id_);
  }
  glBindTexture(GL_TEXTURE_2D, id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB,
               GL_UNSIGNED_BYTE, rgb.data);
  glBindTexture(GL_TEXTURE_2D, 0);
  return true;
}

void Texture::UploadGray(const uint8_t* data, int width, int height) {
  if (id_ == 0) {
    glGenTextures(1, &id_);
  }
  glBindTexture(GL_TEXTURE_2D, id_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED,
               GL_UNSIGNED_BYTE, data);
  // Replicate the single channel across RGB so it renders as gray.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint TagTextureCache::GetForTag(int id) {
  auto it = textures_.find(id);
  if (it == textures_.end()) {
    Texture tex;
    tex.LoadFromFile(dir_ + "/tag_" + std::to_string(id) + ".png");
    it = textures_.emplace(id, std::move(tex)).first;
  }
  return it->second.GetId();
}

}  // namespace viz
