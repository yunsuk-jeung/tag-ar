#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "viewer/gl_loader.hpp"

namespace viz {

class Texture {
 public:
  Texture() = default;
  ~Texture();

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&& other) noexcept;

  bool LoadFromFile(const std::string& path);

  void UploadGray(const uint8_t* data, int width, int height);

  GLuint GetId() const { return id_; }
  bool IsValid() const { return id_ != 0; }

 private:
  GLuint id_ = 0;
};

class TagTextureCache {
 public:
  explicit TagTextureCache(std::string dir) : dir_(std::move(dir)) {}

  GLuint GetForTag(int id);

 private:
  std::string dir_;
  std::unordered_map<int, Texture> textures_;
};

}  // namespace viz
