//
//  TagARKitWrapper.h
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

struct WrapperTagPose {
  int id = 0;
  float T_w_t[16] = {};  // 4x4 tag-to-world, column-major
};

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

  bool Init(float tag_size_m);
  bool InitWithConfigFile(const char* config_path);

  void Start();
  void Stop();

  void SubmitFrameGray(int64_t t_ns, const uint8_t* gray, int width, int height,
                       const float pose[16], const float intrinsics[4]);

  bool GetLatestResult(WrapperResult& out);

  static void InitLogging(const char* log_dir);

  static void Ping();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace tagar
