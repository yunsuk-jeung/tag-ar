#pragma once

#include <cstddef>
#include <string>

namespace tagar {

struct TrackerConfig {
  float tag_size_m = 0.08f;

  static TrackerConfig Load(const std::string& file);
};

}  // namespace tagar
