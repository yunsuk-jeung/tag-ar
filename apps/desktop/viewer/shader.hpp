#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "viewer/gl_loader.hpp"

namespace viz {

// Minimal shader program: loads a vertex + fragment shader from files, links
// them, and exposes uniform lookups. Ported from the svis viewer.
class Shader {
 public:
  Shader() = default;
  ~Shader() {
    if (program_) {
      glDeleteProgram(program_);
    }
  }

  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  bool Load(const std::string& vertex_path, const std::string& fragment_path) {
    GLuint vs = Compile(GL_VERTEX_SHADER, vertex_path);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, fragment_path);
    if (vs == 0 || fs == 0) {
      return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!linked) {
      char log[1024];
      glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
      std::cerr << "Shader link failed: " << log << std::endl;
      glDeleteProgram(program_);
      program_ = 0;
      return false;
    }
    return true;
  }

  void Bind() const { glUseProgram(program_); }
  void Unbind() const { glUseProgram(0); }
  GLuint GetId() const { return program_; }
  GLint GetUniform(const char* name) const {
    return glGetUniformLocation(program_, name);
  }

 private:
  static GLuint Compile(GLenum type, const std::string& path) {
    std::ifstream stream(path, std::ios::in);
    if (!stream.is_open()) {
      std::cerr << "Failed to open shader file: " << path << std::endl;
      return 0;
    }
    std::stringstream ss;
    ss << stream.rdbuf();
    const std::string source = ss.str();
    const char* code = source.c_str();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
      char log[1024];
      glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
      std::cerr << "Shader compile failed (" << path << "): " << log
                << std::endl;
      glDeleteShader(shader);
      return 0;
    }
    return shader;
  }

  GLuint program_ = 0;
};

}  // namespace viz
