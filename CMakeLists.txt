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
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs features2d calib3d highgui)

add_subdirectory(cpp)

if(BUILD_DESKTOP)
  add_subdirectory(apps/desktop)
endif()

# if(BUILD_TESTS)
#   add_subdirectory(cpp/tests)
# endif()
