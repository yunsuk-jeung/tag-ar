#include <memory>
#include <filesystem>
#include <utility>

#include <Eigen/Core>

#include "tagar/tracker.hpp"
#include "tagar/simulator.hpp"
#include "viewer/viewer.hpp"
#include "viewer/mesh_renderer.hpp"

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

  fs::path config_path = project_root / "config/tracker.json";
  if (!tag_tracker->Init(tagar::TrackerConfig::Load(config_path.string()))) {
    return 1;
  }

  simulator->SetFrameCallback([&](tagar::FrameBuffer frame_buffer) {
    tag_tracker->SubmitFrame(std::move(frame_buffer));
  });

  viz::Viewer viewer;
  const std::string shader_dir =
      (project_root / "apps/desktop/viewer/shader").string();
  if (!viewer.Init(1000, 1000, "tag-ar viewer", shader_dir)) {
    return 1;
  }

  viz::MeshRenderer origin_axis;
  origin_axis.Upload(viz::MakeAxis(0.5f));
  origin_axis.SetLineWidth(3.0f);
  const Eigen::Matrix4f kIdentity = Eigen::Matrix4f::Identity();

  while (!viewer.ShouldClose()) {
    viewer.PollEvents();

    // if (file_reader->HasNextFrame()) {
    //   simulator->FeedFrame();
    //   tag_tracker->ProcessOnce();
    // }

    viewer.BeginFrame();
    viewer.Draw(origin_axis, kIdentity);
    viewer.EndFrame();
  }

  return 0;
}
