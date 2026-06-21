#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <utility>

#include "tagar/file_reader.hpp"
#include "tagar/types.hpp"
#include "tagar/logger.hpp"

namespace tagar {

// Replays a recorded dataset through a FileReader on a background thread,
// delivering each frame to a callback. With realtime pacing the frames are
// emitted according to the recording's timestamps (scaled by speed).
class Simulator {
 public:
  using FrameCallback = std::function<void(const FrameBuffer&)>;

  Simulator() = default;
  ~Simulator() { Stop(); }

  // Binds the dataset source and playback options. Call before Start().
  bool Init(FileReader* reader, double speed = 1.0, bool realtime = true) {
    reader_ = reader;
    if (!reader) {
      return false;
    }

    speed_ = speed;
    realtime_ = realtime;
    initialized_ = true;
    return true;
  }

  void SetFrameCallback(FrameCallback callback) {
    frame_callback_ = std::move(callback);
  }
  void SetRealtime(bool realtime) { realtime_ = realtime; }
  void SetSpeed(double speed) { speed_ = speed; }

  void Start() {
    if (!initialized_) {
      return;
    }
    Stop();
    end_ = false;
    reader_->Rewind();
    thread_ = std::thread(&Simulator::FeedFrames, this);
  }

  void Stop() {
    end_ = true;
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  // for debug
  void FeedFrame() {
    if (!frame_callback_) {
      return;
    }

    FrameBuffer frame = reader_->GetNextFrame();
    if (frame.image_buffer.buffer.empty()) {
      LogW("no frame");
    }

    frame_callback_(frame);
  }

 protected:
  void FeedFrames() {
    if (!frame_callback_) {
      return;
    }

    bool initialized = false;
    int64_t start_ts = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (!end_ && reader_->HasNextFrame()) {
      FrameBuffer frame = reader_->GetNextFrame();
      if (frame.image_buffer.buffer.empty()) {
        continue;
      }

      if (!initialized) {
        start_ts = frame.t_ns;
        start_time = std::chrono::steady_clock::now();
        initialized = true;
      }

      if (realtime_) {
        const int64_t dt_ns = frame.t_ns - start_ts;
        const auto wait_ns =
            static_cast<int64_t>(static_cast<double>(dt_ns) / speed_);
        std::this_thread::sleep_until(start_time +
                                      std::chrono::nanoseconds(wait_ns));
      }

      if (end_) {
        break;
      }
      frame_callback_(frame);
    }
  }

 private:
  FileReader* reader_ = nullptr;
  bool initialized_ = false;
  std::atomic<bool> end_{false};
  std::thread thread_;

  FrameCallback frame_callback_;

  double speed_ = 1.0;
  bool realtime_ = true;
};

}  // namespace tagar
