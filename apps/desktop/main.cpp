#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "tagar/simulator.hpp"
#include "tagar/tracker.hpp"
#include "viewer/mesh_renderer.hpp"
#include "viewer/texture.hpp"
#include "viewer/viewer.hpp"

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

  fs::path dataset_path = project_root / "datasets/04";

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

  viz::MeshRenderer cam_axis;
  cam_axis.Upload(viz::MakeAxis(0.1f));
  cam_axis.SetLineWidth(2.0f);

  viz::MeshRenderer cam_frustum;
  cam_frustum.Upload(viz::MakeCameraFrustum(1377.4f, 1377.4f, 1920.0f, 1440.0f,
                                            0.3f, {1.0f, 0.85f, 0.1f}));
  cam_frustum.SetLineWidth(2.0f);

  viz::MeshRenderer tag_quad;
  tag_quad.Upload(viz::MakeQuad({0.2f, 0.8f, 1.0f}));

  viz::TagTextureCache tag_textures((project_root / "assets").string());

  const float kTagHalfSize = 0.02f;
  const Eigen::Matrix4f kIdentity = Eigen::Matrix4f::Identity();

  tag_tracker->Start();
  simulator->Start();

  while (!viewer.ShouldClose()) {
    viewer.PollEvents();

    viewer.BeginFrame();
    viewer.Draw(origin_axis, kIdentity);

    if (auto result = tag_tracker->GetLatestResult()) {
      const Eigen::Matrix4f T_w_c =
          Eigen::Map<const Eigen::Matrix4f>(result->T_w_c.data());
      viewer.Draw(cam_frustum, T_w_c);
      viewer.Draw(cam_axis, T_w_c);

      std::vector<std::pair<Eigen::Matrix4f, GLuint>> tag_draws;
      tag_draws.reserve(result->tags.size());
      for (const auto& tag : result->tags) {
        Eigen::Matrix4f model =
            Eigen::Map<const Eigen::Matrix4f>(tag.T_w_t.data());
        model.topLeftCorner<3, 3>() *= kTagHalfSize;
        const GLuint texture = tag_textures.GetForTag(tag.id);
        viewer.Draw(tag_quad, model, texture);
        tag_draws.emplace_back(model, texture);
      }

      viewer.DrawReprojectionInset(result->gray.data.data(), result->gray.width,
                                   result->gray.height, result->intrinsics,
                                   T_w_c, tag_draws);
    }

    viewer.EndFrame();
  }

  simulator->Stop();
  tag_tracker->Stop();
  return 0;
}
