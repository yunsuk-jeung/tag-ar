#include <memory>

#include "tagar/tracker.hpp"

int main() {
  std::unique_ptr<tagar::Tracker> tag_tracker;
  tag_tracker = std::make_unique<tagar::Tracker>();

  if (!tag_tracker->Init()) {
    return 1;
  }

  return 0;
}