#pragma once

#include <thread>
#include <cstdint>

namespace tagar {
class Tracker {
 public:
  Tracker();
  ~Tracker();

  bool Init();

 protected:
  // void Track();
  // void DetectTags();
  // void EstimatePoses();

 private:
};
}  // namespace tagar