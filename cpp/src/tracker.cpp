#include <mutex>
#include <utility>

#include "tagar/tracker.hpp"
#include "tagar/frame.hpp"
#include "tagar/logger.hpp"

namespace tagar {
Tracker::Tracker() {}

Tracker::~Tracker() {}

bool Tracker::Init() {
  LogI("tracker init success");
  return true;
}

void Tracker::SubmitFrame(FrameBuffer frame_buffer) {
  std::lock_guard<std::mutex> lock(frame_buffer_lock);
  frame_buffer_queue_.push(std::move(frame_buffer));
}

void Tracker::ProcessOnce() {
  FrameBuffer frame_buffer;
  {
    std::lock_guard<std::mutex> lock(frame_buffer_lock);
    frame_buffer = std::move(frame_buffer_queue_.front());
    frame_buffer_queue_.pop();
  }

  Frame frame(frame_buffer);

  LogI("frame t_ns: {}", frame.GetTimestampNs());
}

void Tracker::Process() {}

}  // namespace tagar