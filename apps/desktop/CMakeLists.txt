add_executable(tagar_ar_sample main.cpp)

target_link_libraries(tagar_ar_sample
  PRIVATE
  TagAR::Core
  glfw
  OpenGL::GL
)
