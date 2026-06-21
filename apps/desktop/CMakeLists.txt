add_executable(tagar_ar_sample
  main.cpp
  viewer/viewer.cpp
  viewer/gl_camera.cpp
  viewer/mesh_renderer.cpp
  viewer/texture.cpp
  viewer/render_target.cpp
)

# So that #include "viewer/..." resolves from this directory.
target_include_directories(tagar_ar_sample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(tagar_ar_sample
  PRIVATE
  TagAR::Core
  Eigen3::Eigen
  glfw
  OpenGL::GL
)
