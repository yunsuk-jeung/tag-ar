//
//  wrapper.h
//  TagARKit
//
//  Pure C++ facade over tagar::Tracker. Hides OpenCV/Sophus/Eigen behind a
//  PIMPL so the Objective-C++ layer (TagARKit.mm) only sees plain types.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tagar {

// Bridge-boundary mirror of a tracked tag (no Eigen/Sophus types).
struct WrapperTagPose {
  int id = 0;
  float T_w_t[16] = {};  // 4x4 tag-to-world, column-major
};

// Bridge-boundary mirror of TrackResult.
struct WrapperResult {
  int64_t t_ns = 0;
  float T_w_c[16] = {};  // 4x4 camera-to-world, column-major
  std::vector<WrapperTagPose> tags;
};

class Wrapper {
 public:
  Wrapper();
  ~Wrapper();

  Wrapper(const Wrapper&) = delete;
  Wrapper& operator=(const Wrapper&) = delete;

  bool Init(float tag_size_m, std::size_t pose_buffer_size);
  void Start();
  void Stop();

  // Submit an RGB frame (3 bytes/pixel, row-major). pose is camera-to-world
  // (column-major), intrinsics is [fx, fy, cx, cy].
  void SubmitFrameRGB(int64_t t_ns, const uint8_t* rgb, int width, int height,
                      const float pose[16], const float intrinsics[4]);

  // Copy out the latest result. Returns false if none is available yet.
  bool GetLatestResult(WrapperResult& out);

  // Bridge smoke test.
  static void Ping();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace tagar
