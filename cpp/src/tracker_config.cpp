#include <fstream>

#include <nlohmann/json.hpp>

#include "tagar/tracker_config.hpp"
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

  config.tag_pose_buffer_size =
      json.value("pose_buffer_size", config.tag_pose_buffer_size);
  config.board_markers_x = json.value("board_markers_x", config.board_markers_x);
  config.board_markers_y = json.value("board_markers_y", config.board_markers_y);
  config.marker_length_m =
      json.value("marker_length_m", config.marker_length_m);
  config.marker_separation_m =
      json.value("marker_separation_m", config.marker_separation_m);
  config.filter_alpha = json.value("filter_alpha", config.filter_alpha);
  config.filter_trans_gate_m =
      json.value("filter_trans_gate_m", config.filter_trans_gate_m);
  config.filter_rot_gate_deg =
      json.value("filter_rot_gate_deg", config.filter_rot_gate_deg);
  config.filter_max_rejects =
      json.value("filter_max_rejects", config.filter_max_rejects);

  if (config.tag_pose_buffer_size < 1) {
    config.tag_pose_buffer_size = 1;
  }

  LogI("TrackerConfig.pose_buffer_size: {}", config.tag_pose_buffer_size);
  LogI("TrackerConfig.board: {}x{} marker={}m sep={}m", config.board_markers_x,
       config.board_markers_y, config.marker_length_m,
       config.marker_separation_m);
  return config;
}

}  // namespace tagar
