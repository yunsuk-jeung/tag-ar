#pragma once

#include <cstdint>
#include <vector>

#include <Eigen/Core>

#include "viewer/gl_loader.hpp"

namespace viz {

class Shader;

struct MeshData {
  std::vector<Eigen::Vector3f> positions;
  std::vector<Eigen::Vector3f> colors;
  std::vector<Eigen::Vector2f> uvs;
  std::vector<uint32_t> indices;
  GLenum mode = GL_TRIANGLES;
};

MeshData MakeAxis(float length);

MeshData MakeCameraFrustum(float fx, float fy, float img_w, float img_h,
                           float scale, const Eigen::Vector3f& color);

MeshData MakeQuad(const Eigen::Vector3f& color);

MeshData MakeCube(float half_extent);

class MeshRenderer {
 public:
  MeshRenderer() = default;
  ~MeshRenderer();

  MeshRenderer(const MeshRenderer&) = delete;
  MeshRenderer& operator=(const MeshRenderer&) = delete;

  void Upload(const MeshData& data);

  void Draw(const Shader& shader, const Eigen::Matrix4f& mvp,
            GLuint texture = 0) const;

  void SetLineWidth(float width) { line_width_ = width; }

 private:
  static GLuint WhiteTexture();
  static GLuint white_texture_;

  GLuint vao_ = 0;
  GLuint vbo_pos_ = 0;
  GLuint vbo_col_ = 0;
  GLuint vbo_uv_ = 0;
  GLuint ebo_ = 0;

  GLenum mode_ = GL_TRIANGLES;
  GLsizei vertex_count_ = 0;
  GLsizei index_count_ = 0;

  float line_width_ = 2.0f;
};

}  // namespace viz
