#include "tagar/tracker_config.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "tagar/logger.hpp"

namespace tagar {

TrackerConfig TrackerConfig::Load(const std::string& file) {
  TrackerConfig config;  // starts at defaults

  std::ifstream input(file);
  if (!input.is_open()) {
    LogW("TrackerConfig: cannot open {}, using defaults", file);
    return config;
  }

  nlohmann::json json;
  try {
    input >> json;
  } catch (const std::exception& e) {
    LogE("TrackerConfig: json parse error: {}", e.what());
    return config;
  }

  config.tag_size_m = json.value("tag_size_m", config.tag_size_m);
  config.tag_pose_buffer_size =
      json.value("pose_buffer_size", config.tag_pose_buffer_size);

  if (config.tag_size_m <= 0.0f) {
    config.tag_size_m = 0.08f;
  }
  if (config.tag_pose_buffer_size < 1) {
    config.tag_pose_buffer_size = 1;
  }

  LogI("TrackerConfig.tag_size_m: {}", config.tag_size_m);
  LogI("TrackerConfig.pose_buffer_size: {}", config.tag_pose_buffer_size);
  return config;
}

}  // namespace tagar
