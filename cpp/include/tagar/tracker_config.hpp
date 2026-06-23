#pragma once

#include <cstdint>
#include <string>

namespace tagar {

struct TrackerConfig {
  float tag_size_m = 0.085f;
  int64_t tag_discard_time_threshold = 8e7;
  int target_width = 640;
  // optical-flow fallback for undetected tags
  bool flow_enabled = true;
  float flow_fb_threshold_px = 0.5f;  // forward-backward consistency [px]
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
  float pos_beta = 100.0f;
  float rot_min_cutoff = 1.0f;
  float rot_beta = 0.5f;
  float d_cutoff = 3.0f;

  static TrackerConfig Load(const std::string& file);
};

}  // namespace tagar
