#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "tagar/types.hpp"

namespace cv {
class VideoCapture;
}  // namespace cv

namespace tagar {

// Reads a recording captured by the iOS app: a directory holding one .mp4 video
// and one .json sidecar with per-frame timestamp, 6DoF pose, and intrinsics.
// Mono camera only, no IMU.
class FileReader {
 public:
  FileReader();
  ~FileReader();

  // Scans dataset_dir for the .mp4 and .json files, opens them, and parses the
  // metadata. Returns false if either file is missing or fails to load.
  bool Setup(const std::string& dataset_dir);
  void Reset();

  // Sequential access: decodes the next video frame and pairs it with the
  // matching pose, intrinsics, and timestamp from the metadata.
  bool HasNextFrame() const;
  FrameBuffer GetNextFrame();
  size_t GetFrameCount() const;

  // Recording time range, in nanoseconds.
  int64_t GetStartTimestampNs() const;
  int64_t GetEndTimestampNs() const;

  std::string GetDatasetName() const;

 private:
  // Per-frame metadata parsed from the JSON sidecar.
  struct FrameMeta {
    int64_t t_ns = 0;
    std::array<float, 16> pose;       // 4x4 camera-to-world, column-major
    std::array<float, 4> intrinsics;  // [fx, fy, cx, cy]
  };

  bool ParseMetadataJson(const std::string& json_path);
  bool OpenVideo(const std::string& video_path);
  void ComputeTimestampRange();

  std::string dataset_name_;

  std::vector<FrameMeta> frames_;
  size_t frame_index_ = 0;

  std::unique_ptr<cv::VideoCapture> video_;

  int image_width_ = 0;
  int image_height_ = 0;

  int64_t start_timestamp_ns_ = 0;
  int64_t end_timestamp_ns_ = 0;
  bool initialized_ = false;
};

}  // namespace tagar
