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

  config.tag_size_m = json.value("tag_size_m", config.tag_size_m);

  if (config.tag_size_m <= 0.0f) {
    config.tag_size_m = 0.08f;
  }

  config.tag_discard_time_threshold = json.value(
      "tag_discard_time_threshold", config.tag_discard_time_threshold);
  config.target_width = json.value("target_width", config.target_width);

  config.depth_refine_enabled =
      json.value("depth_refine_enabled", config.depth_refine_enabled);
  config.max_depth_samples =
      json.value("max_depth_samples", config.max_depth_samples);
  config.refine_sigma_px =
      json.value("refine_sigma_px", config.refine_sigma_px);
  config.refine_sigma_z = json.value("refine_sigma_z", config.refine_sigma_z);
  config.refine_huber_m = json.value("refine_huber_m", config.refine_huber_m);
  config.refine_max_iters =
      json.value("refine_max_iters", config.refine_max_iters);

  config.filter_enabled = json.value("filter_enabled", config.filter_enabled);
  config.filter_translation =
      json.value("filter_translation", config.filter_translation);
  config.filter_rotation =
      json.value("filter_rotation", config.filter_rotation);
  config.pos_min_cutoff = json.value("pos_min_cutoff", config.pos_min_cutoff);
  config.pos_beta = json.value("pos_beta", config.pos_beta);
  config.rot_min_cutoff = json.value("rot_min_cutoff", config.rot_min_cutoff);
  config.rot_beta = json.value("rot_beta", config.rot_beta);
  config.d_cutoff = json.value("d_cutoff", config.d_cutoff);

  LogI("TrackerConfig.tag_size_m: {}  filter_enabled: {}", config.tag_size_m,
       config.filter_enabled);
  return config;
}

}  // namespace tagar
