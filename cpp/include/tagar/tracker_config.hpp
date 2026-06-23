#pragma once

#include <cstddef>
#include <string>

namespace tagar {

struct TrackerConfig {
  float tag_size_m = 0.08f;

  bool filter_enabled = true;
  bool filter_translation = true;
  bool filter_rotation = true;
  float pos_min_cutoff = 1.0f;
  float pos_beta = 0.5f;
  float rot_min_cutoff = 1.0f;
  float rot_beta = 0.5f;
  float d_cutoff = 1.0f;

  static TrackerConfig Load(const std::string& file);
};

}  // namespace tagar
