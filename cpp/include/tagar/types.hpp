#pragma once

#include <cstdint>
#include <vector>
#include <array>

namespace tagar {

enum class ColorFormat {
  kUnknown = 0,
  kRGB,
  kNV12,
};

struct ImageBuffer {
  int width = 0;
  int height = 0;
  int length = 0;
  std::vector<uint8_t> buffer;
  ColorFormat format = ColorFormat::kUnknown;
};

struct PoseBuffer {
  std::array<float, 16> buffer;
};

struct FrameBuffer {
  int64_t t_ns;
  ImageBuffer image_buffer;
  PoseBuffer pose_buffer;
};

}  // namespace tagar