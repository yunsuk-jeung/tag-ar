#include <memory>
#include <filesystem>

#include "tagar/tracker.hpp"
#include "tagar/simulator.hpp"

namespace fs = std::filesystem;

int main() {
  std::unique_ptr<tagar::FileReader> file_reader;
  std::unique_ptr<tagar::Simulator> simulator;
  std::unique_ptr<tagar::Tracker> tag_tracker;

  file_reader = std::make_unique<tagar::FileReader>();
  simulator = std::make_unique<tagar::Simulator>();
  tag_tracker = std::make_unique<tagar::Tracker>();

  const auto project_root =
      fs::path(__FILE__).parent_path().parent_path().parent_path();

  fs::path dataset_path = project_root / "datasets/01";

  if (!file_reader->Init(dataset_path.string())) {
    return 1;
  };

  if (!simulator->Init(file_reader.get())) {
    return 1;
  };
  if (!tag_tracker->Init()) {
    return 1;
  }

  simulator->SetFrameCallback([&](tagar::FrameBuffer frame_buffer) {
    return tag_tracker->SubmitFrame(frame_buffer);
  });

  simulator->FeedFrame();

  return 0;
}