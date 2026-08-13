#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/scene_thumbnail/scene_thumbnail_render.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/thumbnail/scene_thumbnail_fingerprint.h"
#include "runtime/resource/thumbnail/thumbnail_placeholders.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_ne_string(const char* label, const eastl::string& a,
                      const eastl::string& b) {
  if (a == b) {
    std::fprintf(stderr, "FAIL %s (strings equal)\n", label);
    ++g_failures;
  }
}

void expect_eq_string(const char* label, const eastl::string& a,
                      const eastl::string& b) {
  if (a != b) {
    std::fprintf(stderr, "FAIL %s\n  a=%s\n  b=%s\n", label, a.c_str(),
                 b.c_str());
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

fs::path makeTempProject() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path root = fs::temp_directory_path() /
                  ("blunder_scene_thumb_fp_" + std::to_string(stamp));
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / "Assets" / "Meshes");
  return root;
}

void writeText(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << text;
}

uint64_t mtimeOf(const fs::path& path) {
  std::error_code ec;
  const auto stamp = fs::last_write_time(path, ec);
  if (ec) {
    return 0;
  }
  return static_cast<uint64_t>(stamp.time_since_epoch().count());
}

void touchFile(const fs::path& path) {
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  const auto now = fs::file_time_type::clock::now();
  fs::last_write_time(path, now);
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  {
    eastl::vector<uint8_t> rgba;
    fillThumbnailPlaceholder(ThumbnailPlaceholderKind::Scene, 32, 32, rgba);
    expect_true("scene placeholder fills rgba", rgba.size() == 32u * 32u * 4u);
  }

  {
    const MeshPreviewCameraFrame frame = meshPreviewFrameFromPlayCamera(
        Vec3(1.0f, 2.0f, 3.0f), Vec3(0.0f, 1.0f, 0.0f), glm::radians(60.0f));
    expect_true("framing ok", frame.ok);
    expect_true("framing eye x", std::fabs(frame.eye.x - 1.0f) < 1e-4f);
    expect_true("framing target y", std::fabs(frame.target.y - 3.0f) < 1e-4f);
    expect_true("framing fov",
                std::fabs(frame.vertical_fov_rad - glm::radians(60.0f)) < 1e-4f);
  }

  {
    const MeshPreviewCameraFrame bad =
        meshPreviewFrameFromPlayCamera(Vec3(0.0f), Vec3(0.0f), 0.0f);
    expect_true("zero fov -> !ok", !bad.ok);
  }

  const fs::path project = makeTempProject();
  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  const fs::path mesh_a = project / "Assets" / "Meshes" / "a.mesh.yaml";
  const fs::path mesh_b = project / "Assets" / "Meshes" / "b.mesh.yaml";
  const fs::path unrelated = project / "Assets" / "Meshes" / "unrelated.txt";
  writeText(mesh_a, "type: Mesh\nsource: a.gltf\n");
  writeText(mesh_b, "type: Mesh\nsource: b.gltf\n");
  writeText(unrelated, "noise\n");

  const fs::path child_scene =
      project / "Assets" / "Scenes" / "child.scene.asset";
  writeText(child_scene,
            "{\n"
            "  \"type\": \"Scene\",\n"
            "  \"entities\": [\n"
            "    {\n"
            "      \"name\": \"ChildMesh\",\n"
            "      \"position\": [0, 0, 0],\n"
            "      \"rotation\": [0, 0, 0],\n"
            "      \"rotationMode\": \"euler_degrees\",\n"
            "      \"mesh\": \"assets/Meshes/b.mesh.yaml\"\n"
            "    }\n"
            "  ]\n"
            "}\n");

  const fs::path root_scene = project / "Assets" / "Scenes" / "root.scene.asset";
  writeText(root_scene,
            "{\n"
            "  \"type\": \"Scene\",\n"
            "  \"entities\": [\n"
            "    {\n"
            "      \"name\": \"RootMesh\",\n"
            "      \"position\": [0, 0, 0],\n"
            "      \"rotation\": [0, 0, 0],\n"
            "      \"rotationMode\": \"euler_degrees\",\n"
            "      \"mesh\": \"assets/Meshes/a.mesh.yaml\"\n"
            "    }\n"
            "  ],\n"
            "  \"childScenes\": [\n"
            "    {\n"
            "      \"scene\": \"assets/Scenes/child.scene.asset\",\n"
            "      \"name\": \"Child\",\n"
            "      \"position\": [0, 0, 0],\n"
            "      \"rotation\": [0, 0, 0],\n"
            "      \"rotationMode\": \"euler_degrees\",\n"
            "      \"scale\": [1, 1, 1]\n"
            "    }\n"
            "  ]\n"
            "}\n");

  const eastl::string root_vp = "assets/Scenes/root.scene.asset";
  const eastl::vector<eastl::string> refs =
      collectSceneDirectMeshReferences(file_system, nullptr, root_vp);
  expect_true("collects only root mesh refs (legacy childScenes ignored)",
              refs.size() == 1);
  if (refs.size() >= 1) {
    expect_eq_string("sorted ref 0", refs[0], "assets/Meshes/a.mesh.yaml");
  }

  const uint64_t scene_mtime = mtimeOf(root_scene);
  const eastl::string fp1 =
      computeSceneThumbnailFingerprint(file_system, nullptr, root_vp, scene_mtime);
  expect_true("fingerprint non-empty", !fp1.empty());

  touchFile(unrelated);
  const eastl::string fp_unrelated =
      computeSceneThumbnailFingerprint(file_system, nullptr, root_vp, scene_mtime);
  expect_eq_string("unrelated file does not change fingerprint", fp1,
                   fp_unrelated);

  touchFile(mesh_b);
  const eastl::string fp_child_mesh =
      computeSceneThumbnailFingerprint(file_system, nullptr, root_vp, scene_mtime);
  expect_eq_string("legacy child mesh mtime does not change fingerprint", fp1,
                   fp_child_mesh);

  touchFile(mesh_a);
  const eastl::string fp_root_mesh =
      computeSceneThumbnailFingerprint(file_system, nullptr, root_vp, scene_mtime);
  expect_ne_string("root mesh mtime changes fingerprint", fp1, fp_root_mesh);

  std::error_code cleanup_ec;
  fs::remove_all(project, cleanup_ec);

  if (g_failures != 0) {
    std::fprintf(stderr, "scene_thumbnail_fingerprint_test: %d failure(s)\n",
                 g_failures);
    g_runtime_global_context.m_logger_system.reset();
    return 1;
  }
  std::printf("scene_thumbnail_fingerprint_test: all passed\n");
  g_runtime_global_context.m_logger_system.reset();
  return 0;
}
