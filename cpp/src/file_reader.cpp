#include "tagar/file_reader.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "tagar/logger.hpp"

namespace fs = std::filesystem;

namespace tagar {

FileReader::FileReader() = default;
FileReader::~FileReader() = default;

bool FileReader::Init(const std::string& dataset_dir) {
  Reset();

  if (!fs::is_directory(dataset_dir)) {
    LogE("FileReader: not a directory: {}", dataset_dir);
    return false;
  }

  // Pick the first .mp4 and .json in the directory.
  std::string video_path;
  std::string json_path;
  for (const auto& entry : fs::directory_iterator(dataset_dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string ext = entry.path().extension().string();
    if (ext == ".mp4" && video_path.empty()) {
      video_path = entry.path().string();
    } else if (ext == ".json" && json_path.empty()) {
      json_path = entry.path().string();
    }
  }

  if (video_path.empty() || json_path.empty()) {
    LogE("FileReader: missing mp4 or json in {} (mp4='{}', json='{}')",
         dataset_dir, video_path, json_path);
    return false;
  }

  if (!ParseMetadataJson(json_path) || !OpenVideo(video_path)) {
    Reset();
    return false;
  }

  dataset_name_ = fs::path(dataset_dir).filename().string();
  ComputeTimestampRange();
  initialized_ = true;
  LogI("FileReader: loaded '{}' ({} frames)", dataset_name_, frames_.size());
  return true;
}

void FileReader::Reset() {
  dataset_name_.clear();
  frames_.clear();
  meta_cursor_ = 0;
  video_.reset();
  video_frame_count_ = 0;
  video_index_ = 0;
  image_width_ = 0;
  image_height_ = 0;
  start_timestamp_ns_ = 0;
  end_timestamp_ns_ = 0;
  initialized_ = false;
}

void FileReader::Rewind() {
  meta_cursor_ = 0;
  video_index_ = 0;
  if (video_) {
    video_->set(cv::CAP_PROP_POS_FRAMES, 0);
  }
}

bool FileReader::ParseMetadataJson(const std::string& json_path) {
  std::ifstream file(json_path);
  if (!file) {
    LogE("FileReader: cannot open json: {}", json_path);
    return false;
  }

  nlohmann::json root;
  try {
    file >> root;
  } catch (const std::exception& e) {
    LogE("FileReader: json parse error: {}", e.what());
    return false;
  }

  image_width_ = root.value("imageWidth", 0);
  image_height_ = root.value("imageHeight", 0);
  const int64_t timescale = root.value("timescale", int64_t{1});
  if (timescale <= 0) {
    LogE("FileReader: invalid timescale: {}", timescale);
    return false;
  }

  const auto& frames = root.at("frames");
  frames_.reserve(frames.size());
  for (const auto& f : frames) {
    FrameMeta meta;
    // CMTime value -> nanoseconds: timestamp / timescale seconds. Use a 128-bit
    // intermediate; t is already ~1e13, so t * 1e9 would overflow int64.
    const int64_t t = f.at("timestamp").get<int64_t>();
    meta.t_ns = static_cast<int64_t>(
        static_cast<__int128>(t) * 1'000'000'000LL / timescale);

    const auto& transform = f.at("transform");
    for (size_t i = 0; i < meta.pose.size(); ++i) {
      meta.pose[i] = transform[i].get<float>();
    }

    const auto& intr = f.at("intrinsics");
    for (size_t i = 0; i < meta.intrinsics.size(); ++i) {
      meta.intrinsics[i] = intr[i].get<float>();
    }

    frames_.push_back(meta);
  }
  return true;
}

bool FileReader::OpenVideo(const std::string& video_path) {
  video_ = std::make_unique<cv::VideoCapture>(video_path);
  if (!video_->isOpened()) {
    LogE("FileReader: cannot open video: {}", video_path);
    return false;
  }
  video_frame_count_ = static_cast<int>(video_->get(cv::CAP_PROP_FRAME_COUNT));
  return true;
}

void FileReader::ComputeTimestampRange() {
  if (frames_.empty()) {
    start_timestamp_ns_ = 0;
    end_timestamp_ns_ = 0;
    return;
  }
  start_timestamp_ns_ = frames_.front().t_ns;
  end_timestamp_ns_ = frames_.back().t_ns;
}

bool FileReader::HasNextFrame() const {
  return initialized_ && video_index_ < video_frame_count_;
}

FrameBuffer FileReader::GetNextFrame() {
  FrameBuffer frame{};
  if (!HasNextFrame()) {
    return frame;
  }

  cv::Mat bgr;
  if (!video_ || !video_->read(bgr) || bgr.empty()) {
    LogW("FileReader: video ended early at frame {}/{}", video_index_,
         video_frame_count_);
    video_index_ = video_frame_count_;  // stop iteration
    return frame;
  }
  // Presentation time of the frame just read, in ms relative to video start.
  const double video_ms = video_->get(cv::CAP_PROP_POS_MSEC);
  ++video_index_;

  // Advance the cursor to the metadata entry closest in time. Both streams are
  // time-ordered, so this is a forward merge; a dropped video frame simply skips
  // the metadata entries that have no matching image.
  const int64_t t0 = frames_.front().t_ns;
  auto rel_ms = [&](size_t i) {
    return (frames_[i].t_ns - t0) / 1e6;
  };
  while (meta_cursor_ + 1 < frames_.size() &&
         std::abs(rel_ms(meta_cursor_ + 1) - video_ms) <=
             std::abs(rel_ms(meta_cursor_) - video_ms)) {
    ++meta_cursor_;
  }
  const FrameMeta& meta = frames_[meta_cursor_];

  frame.t_ns = meta.t_ns;
  frame.pose = meta.pose;
  frame.intrinsics = meta.intrinsics;

  cv::Mat rgb;
  cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

  ImageBuffer& img = frame.image_buffer;
  img.width = rgb.cols;
  img.height = rgb.rows;
  img.length = static_cast<int>(rgb.total() * rgb.elemSize());
  img.format = ColorFormat::kRGB;
  img.buffer.assign(rgb.data, rgb.data + img.length);

  return frame;
}

size_t FileReader::GetFrameCount() const {
  return static_cast<size_t>(video_frame_count_);
}

int64_t FileReader::GetStartTimestampNs() const {
  return start_timestamp_ns_;
}

int64_t FileReader::GetEndTimestampNs() const {
  return end_timestamp_ns_;
}

std::string FileReader::GetDatasetName() const {
  return dataset_name_;
}

}  // namespace tagar
