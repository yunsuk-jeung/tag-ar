#pragma once

#include <cstddef>
#include <string>

namespace tagar {

struct TrackerConfig {
  size_t tag_pose_buffer_size = 20;

  // 3x3 (default) AprilTag GridBoard used as a single rigid bundle.
  int board_markers_x = 3;
  int board_markers_y = 3;
  float marker_length_m = 0.04f;       // each marker's edge length
  float marker_separation_m = 0.008f;  // gap between adjacent markers

  // Outlier-rejecting low-pass pose filter.
  double filter_alpha = 0.15;          // EMA weight on the new measurement
  double filter_trans_gate_m = 0.05;   // reject translation jumps beyond this
  double filter_rot_gate_deg = 60.0;   // reject rotation jumps beyond this
  int filter_max_rejects = 5;          // re-acquire after this many rejects

  static TrackerConfig Load(const std::string& file);
};

}  // namespace tagar
