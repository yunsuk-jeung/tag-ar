//
//  TagARKitWrapper.cpp
//  TagARKit
//

#include "TagARKitWrapper.h"

#include <cstring>
#include <utility>

#include "tagar/logger.hpp"
#include "tagar/tracker.hpp"

namespace tagar {

struct Wrapper::Impl {
  Tracker tracker;
};

Wrapper::Wrapper() : impl_(std::make_unique<Impl>()) {}

Wrapper::~Wrapper() = default;

bool Wrapper::Init(float tag_size_m, std::size_t pose_buffer_size) {
  TrackerConfig config;
  config.tag_size_m = tag_size_m;
  config.tag_pose_buffer_size = pose_buffer_size;
  return impl_->tracker.Init(config);
}

bool Wrapper::InitWithConfigFile(const char* config_path) {
  TrackerConfig config =
      TrackerConfig::Load(config_path ? config_path : std::string());
  return impl_->tracker.Init(config);
}

void Wrapper::Start() { impl_->tracker.Start(); }

void Wrapper::Stop() { impl_->tracker.Stop(); }

void Wrapper::SubmitFrameRGB(int64_t t_ns, const uint8_t* rgb, int width,
                             int height, const float pose[16],
                             const float intrinsics[4]) {
  const int length = width * height * 3;

  FrameBuffer frame_buffer;
  frame_buffer.t_ns = t_ns;
  frame_buffer.image_buffer.width = width;
  frame_buffer.image_buffer.height = height;
  frame_buffer.image_buffer.length = length;
  frame_buffer.image_buffer.format = ColorFormat::kRGB;
  frame_buffer.image_buffer.buffer.assign(rgb, rgb + length);
  std::memcpy(frame_buffer.pose.data(), pose, sizeof(float) * 16);
  std::memcpy(frame_buffer.intrinsics.data(), intrinsics, sizeof(float) * 4);

  impl_->tracker.SubmitFrame(std::move(frame_buffer));
}

bool Wrapper::GetLatestResult(WrapperResult& out) {
  std::shared_ptr<const TrackResult> result = impl_->tracker.GetLatestResult();
  if (!result) {
    return false;
  }

  out.t_ns = result->t_ns;
  std::memcpy(out.T_w_c, result->T_w_c.data(), sizeof(float) * 16);

  out.tags.clear();
  out.tags.reserve(result->tags.size());
  for (const TagObservation& tag : result->tags) {
    WrapperTagPose pose;
    pose.id = tag.id;
    std::memcpy(pose.T_w_t, tag.T_w_t.data(), sizeof(float) * 16);
    out.tags.push_back(pose);
  }
  return true;
}

void Wrapper::Ping() { LogI("[TagARKit] wrapper bridge OK"); }

}  // namespace tagar
