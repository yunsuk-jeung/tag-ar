#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <memory>
#include <unordered_map>

#include <opencv2/objdetect/aruco_detector.hpp>

#include "tagar/tag.hpp"
#include "tagar/tracker_config.hpp"
#include "tagar/types.hpp"

namespace tagar {
class Tracker {
 public:
  Tracker();
  ~Tracker();

  bool Init(const TrackerConfig& config);
  void SubmitFrame(FrameBuffer frame_buffer);

  void ProcessOnce();

  std::shared_ptr<const TrackResult> GetLatestResult() const;

 protected:
  void Process();
  // void Track();
  // void DetectTags();
  // void EstimatePoses();

 private:
  std::thread thread_;
  std::mutex frame_buffer_lock;
  std::queue<FrameBuffer> frame_buffer_queue_;

  cv::aruco::ArucoDetector detector_;

  std::unordered_map<int, Tag> tags_;

  TrackerConfig config_;

  mutable std::mutex result_mutex_;
  std::shared_ptr<const TrackResult> latest_result_;
};
}  // namespace tagar