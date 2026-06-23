#pragma once

#include <cstdint>
#include <string>

namespace tagar {

struct TrackerConfig {
  float tag_size_m = 0.08f;
  int64_t tag_discard_time_threshold = 5e8;
  // tag pose optimization
  bool depth_refine_enabled = true;
  int max_depth_samples = 256;
  float refine_sigma_px = 1.0f;  // corner detection noise [px]
  float refine_sigma_z = 0.01f;  // depth noise [m]
  float refine_huber_m = 0.02f;  // depth robust threshold [m]
  int refine_max_iters = 5;      // Gauss-Newton iteration cap
  // tag pose filter
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
