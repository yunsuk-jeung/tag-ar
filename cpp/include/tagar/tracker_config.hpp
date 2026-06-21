#pragma once

#include <cstddef>
#include <string>

namespace tagar {

struct TrackerConfig {
  float tag_size_m = 0.08f;
  size_t tag_pose_buffer_size = 20;

  static TrackerConfig Load(const std::string& file);
};

}  // namespace tagar
