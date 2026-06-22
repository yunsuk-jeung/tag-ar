#include "viewer/mesh_renderer.hpp"
#include "viewer/shader.hpp"

namespace viz {

MeshData MakeAxis(float length) {
  MeshData m;
  m.mode = GL_LINES;
  m.positions = {
      {0, 0, 0}, {length, 0, 0},  // X
      {0, 0, 0}, {0, length, 0},  // Y
      {0, 0, 0}, {0, 0, length},  // Z
  };
  m.colors = {
      {1, 0, 0}, {1, 0, 0},  // X red
      {0, 1, 0}, {0, 1, 0},  // Y green
      {0, 0, 1}, {0, 0, 1},  // Z blue
  };
  return m;
}

MeshData MakeCameraFrustum(float fx, float fy, float img_w, float img_h,
                           float scale, const Eigen::Vector3f& color) {
  const float hw = scale * img_w / (2.0f * fx);
  const float hh = scale * img_h / (2.0f * fy);
  const float z = scale;

  MeshData m;
  m.mode = GL_LINES;
  m.positions = {
      {0, 0, 0},     // 0: apex
      {hw, hh, z},   // 1: top-right
      {hw, -hh, z},  // 2: bottom-right
      {-hw, hh, z},  // 3: top-left
      {-hw, -hh, z}  // 4: bottom-left
  };
  m.colors.assign(m.positions.size(), color);
  m.indices = {
      0, 1, 0, 2, 0, 3, 0, 4,  // apex to the four corners
      1, 2, 2, 4, 4, 3, 3, 1   // far-plane rectangle
  };
  return m;
}

MeshData MakeQuad(const Eigen::Vector3f& color) {
  MeshData m;
  m.mode = GL_TRIANGLES;
  m.positions = {
      {-1, -1, 0},  // 0: bottom-left
      {1, -1, 0},   // 1: bottom-right
      {1, 1, 0},    // 2: top-right
      {-1, 1, 0}    // 3: top-left
  };
  m.uvs = {
      {0, 1},  // bottom-left
      {1, 1},  // bottom-right
      {1, 0},  // top-right
      {0, 0}   // top-left
  };
  m.colors.assign(m.positions.size(), color);
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

MeshData MakeCube(float half_extent) {
  const float h = half_extent;
  // Each face has its own 4 vertices so it can carry a flat, distinct color.
  MeshData m;
  m.mode = GL_TRIANGLES;
  m.positions = {
      {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h},   // +Z front
      {h, -h, -h},  {-h, -h, -h}, {-h, h, -h}, {h, h, -h}, // -Z back
      {h, -h, h},   {h, -h, -h}, {h, h, -h},  {h, h, h},   // +X right
      {-h, -h, -h}, {-h, -h, h}, {-h, h, h},  {-h, h, -h}, // -X left
      {-h, h, h},   {h, h, h},   {h, h, -h},  {-h, h, -h}, // +Y top
      {-h, -h, -h}, {h, -h, -h}, {h, -h, h},  {-h, -h, h}  // -Y bottom
  };
  m.colors = {
      {0.1f, 0.9f, 0.9f},  {0.1f, 0.9f, 0.9f},  {0.1f, 0.9f, 0.9f},  {0.1f, 0.9f, 0.9f},   // +Z front (textured)
      {0.9f, 0.1f, 0.9f},  {0.9f, 0.1f, 0.9f},  {0.9f, 0.1f, 0.9f},  {0.9f, 0.1f, 0.9f},   // -Z back
      {0.9f, 0.2f, 0.2f},  {0.9f, 0.2f, 0.2f},  {0.9f, 0.2f, 0.2f},  {0.9f, 0.2f, 0.2f},   // +X right
      {0.2f, 0.8f, 0.2f},  {0.2f, 0.8f, 0.2f},  {0.2f, 0.8f, 0.2f},  {0.2f, 0.8f, 0.2f},   // -X left
      {0.2f, 0.4f, 1.0f},  {0.2f, 0.4f, 1.0f},  {0.2f, 0.4f, 1.0f},  {0.2f, 0.4f, 1.0f},   // +Y top
      {1.0f, 0.85f, 0.1f}, {1.0f, 0.85f, 0.1f}, {1.0f, 0.85f, 0.1f}, {1.0f, 0.85f, 0.1f}   // -Y bottom
  };
  m.indices = {
      0,  1,  2,  0,  2,  3,   // +Z front
      4,  5,  6,  4,  6,  7,   // -Z back
      8,  9,  10, 8,  10, 11,  // +X right
      12, 13, 14, 12, 14, 15,  // -X left
      16, 17, 18, 16, 18, 19,  // +Y top
      20, 21, 22, 20, 22, 23   // -Y bottom
  };
  return m;
}

MeshRenderer::~MeshRenderer() {
  if (vbo_pos_) {
    glDeleteBuffers(1, &vbo_pos_);
  }
  if (vbo_col_) {
    glDeleteBuffers(1, &vbo_col_);
  }
  if (vbo_uv_) {
    glDeleteBuffers(1, &vbo_uv_);
  }
  if (ebo_) {
    glDeleteBuffers(1, &ebo_);
  }
  if (vao_) {
    glDeleteVertexArrays(1, &vao_);
  }
}

void MeshRenderer::Upload(const MeshData& data) {
  mode_ = data.mode;
  vertex_count_ = static_cast<GLsizei>(data.positions.size());
  index_count_ = static_cast<GLsizei>(data.indices.size());

  if (vao_ == 0) {
    glGenVertexArrays(1, &vao_);
  }
  glBindVertexArray(vao_);

  if (vbo_pos_ == 0) {
    glGenBuffers(1, &vbo_pos_);
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
  glBufferData(GL_ARRAY_BUFFER, data.positions.size() * sizeof(Eigen::Vector3f),
               data.positions.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  if (!data.colors.empty()) {
    if (vbo_col_ == 0) {
      glGenBuffers(1, &vbo_col_);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo_col_);
    glBufferData(GL_ARRAY_BUFFER, data.colors.size() * sizeof(Eigen::Vector3f),
                 data.colors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);
  }

  if (!data.uvs.empty()) {
    if (vbo_uv_ == 0) {
      glGenBuffers(1, &vbo_uv_);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo_uv_);
    glBufferData(GL_ARRAY_BUFFER, data.uvs.size() * sizeof(Eigen::Vector2f),
                 data.uvs.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(2);
  }

  if (index_count_ > 0) {
    if (ebo_ == 0) {
      glGenBuffers(1, &ebo_);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 data.indices.size() * sizeof(uint32_t), data.indices.data(),
                 GL_STATIC_DRAW);
  }

  glBindVertexArray(0);
}

void MeshRenderer::Draw(const Shader& shader, const Eigen::Matrix4f& mvp,
                        GLuint texture) const {
  if (vao_ == 0) {
    return;
  }

  shader.Bind();
  glUniformMatrix4fv(shader.GetUniform("uMVP"), 1, GL_FALSE, mvp.data());
  glUniform1i(shader.GetUniform("uUseTexture"), texture != 0 ? 1 : 0);
  if (texture != 0) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(shader.GetUniform("uTexture"), 0);
  }

  if (mode_ == GL_LINES) {
    glLineWidth(line_width_);
  }

  glBindVertexArray(vao_);
  if (index_count_ > 0) {
    glDrawElements(mode_, index_count_, GL_UNSIGNED_INT, nullptr);
  } else {
    glDrawArrays(mode_, 0, vertex_count_);
  }
  glBindVertexArray(0);
}

}  // namespace viz
