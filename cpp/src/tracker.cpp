#include "tagar/tracker.hpp"

#include "tagar/logger.hpp"

namespace tagar {
Tracker::Tracker() {}

Tracker::~Tracker() {}

bool Tracker::Init() {
  LogI("tracker init success");
  return true;
}

}  // namespace tagar