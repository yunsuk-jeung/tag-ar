#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace tagar {

enum class ColorFormat {
  kUnknown = 0,
  kGray,
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

struct DepthBuffer {
  int width = 0;
  int height = 0;
  std::vector<float> data;  // row-major, meters
};

struct FrameBuffer {
  int64_t t_ns;
  ImageBuffer image_buffer;
  DepthBuffer depth_buffer;         // optional; empty if not captured
  std::array<float, 16> pose;       // 4x4 camera-to-world, column-major
  std::array<float, 4> intrinsics;  // [fx, fy, cx, cy]
};

// Single-channel image as a plain buffer (no OpenCV types).
struct GrayImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> data;  // row-major, 1 byte per pixel
};

struct DepthImage {
  int width = 0;
  int height = 0;
  std::vector<float> data;  // row-major, meters; empty if unavailable
};

// A tracked tag, decoupled from Sophus.
struct TagObservation {
  int id = 0;
  std::array<float, 16> T_w_t{};  // 4x4 tag-to-world, column-major
};

// Snapshot the tracker publishes for consumers
struct TrackResult {
  int64_t t_ns = 0;
  std::array<float, 16> T_w_c{};      // 4x4 camera-to-world, column-major
  std::array<float, 4> intrinsics{};  // [fx, fy, cx, cy] for the gray image
  GrayImage gray;
  DepthImage depth;  // optional; empty if the frame carried no depth
  std::vector<TagObservation> tags;
};

}  // namespace tagar