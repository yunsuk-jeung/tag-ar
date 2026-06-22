cmake_minimum_required(VERSION 3.20)

project(
  TagAR
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Always emit compile_commands.json so clangd indexing stays in sync.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_DESKTOP "Build desktop application" ON)
option(BUILD_TESTS "Build tests" ON)

# Default to an optimized build. Single-config generators leave
# CMAKE_BUILD_TYPE empty, which produces no -O flags at all (the "feels like
# debug" slowness). The from-source deps are only built for Release
# (libs/<os>/release), so Release is also the only type that links cleanly.
# Use -DCMAKE_BUILD_TYPE=RelWithDebInfo for an optimized build that keeps
# debug symbols, or =Debug for an unoptimized one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  message(STATUS "No build type specified; defaulting to Release")
endif()

# Detect target OS for the per-OS dependency layout (libs/<os>/<buildtype>)
# Must match the names used by scripts/build_dependencies_*.sh: mac | ubuntu | ios
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(DEPS_OS "ios")
elseif(APPLE)
  set(DEPS_OS "mac")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(DEPS_OS "ubuntu")
else()
  message(FATAL_ERROR "Unsupported target OS: ${CMAKE_SYSTEM_NAME}")
endif()

# Set installation paths based on build type
# For RelWithDebInfo, use Release libraries (no need for debug symbols in external deps)
string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)
if(BUILD_TYPE_LOWER STREQUAL "debug")
  set(DEPS_INSTALL_DIR "${CMAKE_SOURCE_DIR}/libs/${DEPS_OS}/debug")
else()
  # Use Release libraries for both Release and RelWithDebInfo builds
  set(DEPS_INSTALL_DIR "${CMAKE_SOURCE_DIR}/libs/${DEPS_OS}/release")
endif()

# Map RelWithDebInfo to Release for all imported targets
# This tells CMake to use Release configuration when RelWithDebInfo is not available
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release "")

# Find installed dependencies
set(CMAKE_PREFIX_PATH "${DEPS_INSTALL_DIR}" ${CMAKE_PREFIX_PATH})

find_package(Eigen3 REQUIRED)
find_package(Sophus REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio calib3d objdetect highgui)

# Desktop viewer rendering stack. macOS exposes the OpenGL 4.1 core API
# directly via OpenGL.framework, so no GL loader is needed there. On Linux the
# viewer pulls the core API through GLEW (see apps/desktop/viewer/gl_loader.hpp).
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
if(NOT APPLE)
  # Force module mode: GLEW's own glew-config.cmake exports GLEW::glew, but
  # CMake's FindGLEW gives the conventional GLEW::GLEW target we link against.
  find_package(GLEW REQUIRED MODULE)
endif()

add_subdirectory(cpp)

if(BUILD_DESKTOP)
  add_subdirectory(apps/desktop)
endif()

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(cpp/tests)
endif()
