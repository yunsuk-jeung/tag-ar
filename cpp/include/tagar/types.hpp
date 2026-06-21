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

struct FrameBuffer {
  int64_t t_ns;
  ImageBuffer image_buffer;
  std::array<float, 16> pose;       // 4x4 camera-to-world, column-major
  std::array<float, 4> intrinsics;  // [fx, fy, cx, cy]
};

}  // namespace tagar