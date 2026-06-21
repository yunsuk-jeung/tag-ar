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

void MeshRenderer::Draw(const Shader& shader,
                        const Eigen::Matrix4f& mvp) const {
  if (vao_ == 0) {
    return;
  }

  shader.Bind();
  glUniformMatrix4fv(shader.GetUniform("uMVP"), 1, GL_FALSE, mvp.data());
  glUniform1i(shader.GetUniform("uUseTexture"), use_texture_ ? 1 : 0);
  if (use_texture_) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
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
