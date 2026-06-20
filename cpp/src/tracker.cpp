#include "tagar/tracker.hpp"
#include "tagar/logger.hpp"

namespace tagar {
Tracker::Tracker() {}

Tracker::~Tracker() {}

bool Tracker::Init() {
  LogI("tracker init success");
  return true;
}

void Tracker::SubmitFrame(FrameBuffer frame) {}

void Tracker::Process() {}
}  // namespace tagar