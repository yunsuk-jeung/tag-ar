#pragma once

// Single place that pulls in the OpenGL + GLFW headers in the right order.
//
// macOS exposes the full OpenGL 4.1 core API directly through
// OpenGL.framework (<OpenGL/gl3.h>), so no GL loader is needed there. A
// Linux/Windows build would need a loader such as GLEW/GLAD.
#define GLFW_INCLUDE_NONE
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>
