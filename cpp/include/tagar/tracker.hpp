#pragma once

#include <cstdint>
#include <thread>

#include "tagar/types.hpp"

namespace tagar {
class Tracker {
 public:
  Tracker();
  ~Tracker();

  bool Init();
  void SubmitFrame(FrameBuffer frame);

 protected:
  void Process();
  // void Track();
  // void DetectTags();
  // void EstimatePoses();

 private:
  std::thread thread_;
};
}  // namespace tagar