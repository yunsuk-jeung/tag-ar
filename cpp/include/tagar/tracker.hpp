#pragma once

#include <thread>
#include <queue>
#include <mutex>

#include "tagar/types.hpp"

namespace tagar {
class Tracker {
 public:
  Tracker();
  ~Tracker();

  bool Init();
  void SubmitFrame(FrameBuffer frame_buffer);

  void ProcessOnce();

 protected:
  void Process();
  // void Track();
  // void DetectTags();
  // void EstimatePoses();

 private:
  std::thread thread_;
  std::mutex frame_buffer_lock;
  std::queue<FrameBuffer> frame_buffer_queue_;
};
}  // namespace tagar