cmake_minimum_required(VERSION 3.20)

project(
  TagAR
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_DESKTOP "Build desktop application" ON)
option(BUILD_TESTS "Build tests" ON)

add_subdirectory(cpp)

if(BUILD_DESKTOP)
  add_subdirectory(apps/desktop)
endif()

if(BUILD_TESTS)
  add_subdirectory(cpp/tests)
endif()
