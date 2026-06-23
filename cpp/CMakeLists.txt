add_library(
  tagar_core
)

target_sources(
  tagar_core
  PRIVATE
  src/tracker.cpp
  src/tag.cpp
  src/frame.cpp
  src/file_reader.cpp
  src/tracker_config.cpp
)

target_include_directories(
  tagar_core
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
  tagar_core
  PUBLIC
  Eigen3::Eigen
  Sophus::Sophus
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  ${OpenCV_LIBS}
)

target_compile_features(
  tagar_core
  PUBLIC
  cxx_std_20
)

# Desktop default log location (project-root/logs). On iOS this macro is
# irrelevant: the host app passes a sandbox-writable directory to Logger::Init().
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  # PUBLIC so consumers that include logger.hpp (e.g. the desktop app) resolve
  # the same default log directory.
  target_compile_definitions(
    tagar_core
    PUBLIC
    tagar_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )
endif()

add_library(TagAR::Core ALIAS tagar_core)
