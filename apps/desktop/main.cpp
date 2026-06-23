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

int main(int argc, char** argv) {
  std::unique_ptr<tagar::FileReader> file_reader;
  std::unique_ptr<tagar::Simulator> simulator;
  std::unique_ptr<tagar::Tracker> tag_tracker;

  file_reader = std::make_unique<tagar::FileReader>();
  simulator = std::make_unique<tagar::Simulator>();
  tag_tracker = std::make_unique<tagar::Tracker>();

  const auto project_root =
      fs::path(__FILE__).parent_path().parent_path().parent_path();

  fs::path dataset_path =
      argc > 1 ? fs::path(argv[1]) : project_root / "datasets/09";

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

  const float kCubeHalf = 0.03f;
  viz::MeshRenderer tag_cube;
  tag_cube.Upload(viz::MakeCube(kCubeHalf));

  viz::TagTextureCache tag_textures((project_root / "assets").string());

  viz::MeshRenderer depth_cloud;
  depth_cloud.SetPointSize(3.0f);

  const Eigen::Matrix4f kIdentity = Eigen::Matrix4f::Identity();

  const bool kStepMode = false;

  if (!kStepMode) {
    tag_tracker->Start();
    simulator->Start();
  }

  while (!viewer.ShouldClose()) {
    viewer.PollEvents();

    if (kStepMode && viewer.ConsumeKeyPress(GLFW_KEY_RIGHT)) {
      simulator->FeedFrame();
      tag_tracker->ProcessOnce();
    }

    viewer.BeginFrame();
    viewer.Draw(origin_axis, kIdentity);

    if (auto result = tag_tracker->GetLatestResult()) {
      const Eigen::Matrix4f T_w_c =
          Eigen::Map<const Eigen::Matrix4f>(result->T_w_c.data());
      viewer.Draw(cam_frustum, T_w_c);
      viewer.Draw(cam_axis, T_w_c);

      const Eigen::Vector3f t_w_c = T_w_c.col(3).head<3>();
      const Eigen::Vector3f cam_up = -T_w_c.col(1).head<3>();

      std::vector<std::pair<Eigen::Matrix4f, GLuint>> inset_draws;
      inset_draws.reserve(result->tags.size());
      for (const auto& tag : result->tags) {
        const Eigen::Map<const Eigen::Matrix4f> T_w_t(tag.T_w_t.data());
        const Eigen::Vector3f t_w_t = T_w_t.col(3).head<3>();
        const GLuint texture = tag_textures.GetForTag(tag.id);

        const Eigen::Vector3f z = (t_w_c - t_w_t).normalized();
        const Eigen::Vector3f x = cam_up.cross(z).normalized();
        const Eigen::Vector3f y = z.cross(x);
        Eigen::Matrix4f T_w_cube = Eigen::Matrix4f::Identity();
        T_w_cube.col(0).head<3>() = x;
        T_w_cube.col(1).head<3>() = y;
        T_w_cube.col(2).head<3>() = z;
        T_w_cube.col(3).head<3>() = t_w_t;

        Eigen::Matrix4f T_w_cube_offset = T_w_t;
        T_w_cube_offset.col(3).head<3>() += kCubeHalf * T_w_t.col(2).head<3>();

        // viewer.Draw(tag_cube, T_w_cube, texture);
        viewer.Draw(tag_cube, T_w_cube_offset, texture);

        // inset_draws.push_back({T_w_cube, texture});
        inset_draws.push_back({T_w_cube_offset, texture});
      }

      viewer.DrawReprojectionInset(result->gray.data.data(), result->gray.width,
                                   result->gray.height, result->intrinsics,
                                   T_w_c, tag_cube, inset_draws);

      if (!result->depth.data.empty() && result->gray.width > 0 &&
          result->gray.height > 0) {
        // Depth intrinsics = color intrinsics scaled to the depth resolution.
        const float sx = static_cast<float>(result->depth.width) /
                         static_cast<float>(result->gray.width);
        const float sy = static_cast<float>(result->depth.height) /
                         static_cast<float>(result->gray.height);
        depth_cloud.Upload(viz::MakePointCloud(
            result->depth.data.data(), result->depth.width,
            result->depth.height, result->intrinsics[0] * sx,
            result->intrinsics[1] * sy, result->intrinsics[2] * sx,
            result->intrinsics[3] * sy));
        viewer.Draw(depth_cloud, T_w_c);
      }
    }

    viewer.EndFrame();
  }

  simulator->Stop();
  tag_tracker->Stop();
  return 0;
}
