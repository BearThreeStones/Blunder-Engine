#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

bool writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << text;
  return static_cast<bool>(out);
}

fs::path fixtureGltfPath() {
  return fs::path(BLUNDER_REPO_ROOT) /
         "engine/Resources/Fixtures/dogwalk_test_rig/dogwalk_test_rig.gltf";
}

bool importTestRigIntoProject(const fs::path& project_root,
                              eastl::string& out_mesh_guid,
                              eastl::vector<eastl::pair<eastl::string, eastl::string>>&
                                  out_clip_bindings) {
  using namespace Blunder;
  ensureLogger();

  const fs::path fixture = fixtureGltfPath();
  if (!fs::exists(fixture)) {
    std::fprintf(stderr, "fixture missing: %s\n", fixture.generic_string().c_str());
    return false;
  }

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project_root;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  settings.animations = true;
  const ImportResult mesh_result =
      import_service.importMesh(fixture, "assets/Meshes", settings);
  if (!mesh_result.success || mesh_result.guid.empty()) {
    std::fprintf(stderr, "mesh import failed\n");
    import_service.shutdown();
    registry.shutdown();
    file_system.shutdown();
    return false;
  }

  out_mesh_guid = mesh_result.guid;
  out_clip_bindings.clear();
  for (const ImportResult& clip : mesh_result.animation_clips) {
    if (!clip.success || clip.guid.empty()) {
      continue;
    }
    eastl::string clip_name;
    eastl::string desc_rel = clip.descriptor_virtual_path;
    if (desc_rel.compare(0, 7, "assets/") == 0) {
      desc_rel.erase(0, 7);
    }
    const fs::path descriptor_absolute =
        file_system.resolveAsset(fs::path(desc_rel.c_str()));
    eastl::string descriptor_yaml;
    if (file_system.readText(descriptor_absolute, descriptor_yaml)) {
      AnimationClipAssetDescriptor descriptor{};
      if (AssetYaml::parseAnimationClipDescriptor(descriptor_yaml, descriptor)) {
        eastl::string intermediate_rel = descriptor.source;
        if (intermediate_rel.compare(0, 10, "resources/") == 0) {
          intermediate_rel.erase(0, 10);
        }
        const fs::path intermediate_absolute =
            file_system.resolveResource(fs::path(intermediate_rel.c_str()));
        clip_name = intermediate_absolute.stem().generic_string().c_str();
        eastl::string intermediate_yaml;
        if (file_system.readText(intermediate_absolute, intermediate_yaml)) {
          AnimationClipData clip_data{};
          if (AssetYaml::parseAnimationClipData(intermediate_yaml, clip_data) &&
              !clip_data.name.empty()) {
            clip_name = clip_data.name;
          }
        }
      }
    }
    if (clip_name.empty()) {
      clip_name = fs::path(desc_rel.c_str()).stem().generic_string().c_str();
      const char* kAnimationSuffix = ".animation";
      if (clip_name.size() > 10 &&
          clip_name.compare(clip_name.size() - 10, 10, kAnimationSuffix) == 0) {
        clip_name.erase(clip_name.size() - 10);
      }
    }
    out_clip_bindings.push_back({clip_name, clip.guid});
  }

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  return out_clip_bindings.size() >= 2;
}

bool writeDogwalkTestRigScene(const fs::path& project_root,
                              const eastl::string& mesh_guid,
                              const eastl::vector<eastl::pair<eastl::string, eastl::string>>&
                                  clip_bindings) {
  using namespace Blunder;

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project_root;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  Scene scene;
  scene.setGuid("f1a2b3c4-d5e6-4789-a012-3456789abcde");
  scene.setName("dogwalk_test_rig");

  SceneEntityDefinition sun;
  sun.name = "Sun";
  sun.position = Vec3(96.3266f, 70.2041f, 10.0f);
  scene.getEntities().push_back(eastl::move(sun));

  SceneEntityDefinition camera_rig;
  camera_rig.name = "CameraRig";
  camera_rig.position = Vec3(0.0f, 0.0f, 5.0f);
  camera_rig.parent_name = "Sun";
  scene.getEntities().push_back(eastl::move(camera_rig));

  SceneEntityDefinition character;
  character.name = "TestRig";
  character.position = Vec3(0.0f, 0.0f, 0.0f);
  character.mesh_virtual_path = mesh_guid;
  character.has_skeleton = true;
  for (const auto& binding : clip_bindings) {
    SceneEntityDefinition::AnimationClipBinding clip_binding;
    clip_binding.name = binding.first;
    clip_binding.guid = binding.second;
    character.animation_player_clips.push_back(eastl::move(clip_binding));
  }
  scene.getEntities().push_back(eastl::move(character));

  SceneEntityDefinition camera;
  camera.name = "Main Camera";
  camera.position = Vec3(0.0f, -8.0f, 8.0f);
  camera.rotation = glm::quat(glm::vec3(glm::radians(30.0f), 0.0f, 0.0f));
  camera.has_camera = true;
  camera.camera.vertical_fov_degrees = 45.0f;
  camera.camera.near_clip = 0.1f;
  camera.camera.far_clip = 1000.0f;
  camera.camera.is_main = true;
  scene.getEntities().push_back(eastl::move(camera));

  eastl::string json;
  if (!SceneSerializer::serialize(scene, json, &registry)) {
    registry.shutdown();
    file_system.shutdown();
    return false;
  }

  constexpr const char* kSceneVirtual = "assets/Scenes/dogwalk_test_rig.scene.asset";
  const fs::path scene_absolute =
      file_system.resolveAsset(fs::path("Scenes/dogwalk_test_rig.scene.asset"));
  if (!writeTextFile(scene_absolute, eastl::string(json.c_str()).c_str())) {
    registry.shutdown();
    file_system.shutdown();
    return false;
  }

  registry.ensureSceneAssetRegistered(eastl::string(kSceneVirtual));
  registry.shutdown();
  file_system.shutdown();
  return true;
}

void selfTestImportFixture() {
  using namespace Blunder;
  ensureLogger();

  const fs::path fixture = fixtureGltfPath();
  expect_true("fixture glTF exists", fs::exists(fixture));

  const fs::path temp =
      fs::temp_directory_path() /
      ("blunder_dogwalk_import_self_" + std::to_string(std::time(nullptr)));
  fs::create_directories(temp / "Assets" / "Meshes");
  fs::create_directories(temp / "Assets" / "Animations");
  fs::create_directories(temp / "Resources" / "Models");
  fs::create_directories(temp / ".blunder");

  eastl::string mesh_guid;
  eastl::vector<eastl::pair<eastl::string, eastl::string>> clip_bindings;
  expect_true("self-test import succeeds",
              importTestRigIntoProject(temp, mesh_guid, clip_bindings));
  expect_true("self-test mesh guid", !mesh_guid.empty());
  expect_true("self-test two clips", clip_bindings.size() >= 2);

  bool saw_idle = false;
  bool saw_walk = false;
  for (const auto& binding : clip_bindings) {
    if (binding.first == "idle") {
      saw_idle = true;
    }
    if (binding.first == "walk") {
      saw_walk = true;
    }
  }
  expect_true("self-test idle clip", saw_idle);
  expect_true("self-test walk clip", saw_walk);

  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(temp);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::strcmp(argv[1], "--self-test") == 0) {
    selfTestImportFixture();
    if (g_failures != 0) {
      std::fprintf(stderr, "dogwalk_import_test_rig: %d failure(s)\n", g_failures);
      Blunder::g_runtime_global_context.m_logger_system.reset();
      return 1;
    }
    std::fprintf(stdout, "dogwalk_import_test_rig: self-test passed\n");
    Blunder::g_runtime_global_context.m_logger_system.reset();
    return 0;
  }

  const char* project_arg =
      (argc >= 2) ? argv[1] : "E:/Blunder Projects/Test";
  const fs::path project_root = fs::path(project_arg);

  eastl::string mesh_guid;
  eastl::vector<eastl::pair<eastl::string, eastl::string>> clip_bindings;
  if (!importTestRigIntoProject(project_root, mesh_guid, clip_bindings)) {
    std::fprintf(stderr, "dogwalk_import_test_rig: import failed for %s\n",
                 project_root.generic_string().c_str());
    return 1;
  }

  if (!writeDogwalkTestRigScene(project_root, mesh_guid, clip_bindings)) {
    std::fprintf(stderr, "dogwalk_import_test_rig: scene write failed\n");
    return 1;
  }

  std::fprintf(stdout, "dogwalk_import_test_rig: imported mesh %s\n",
               mesh_guid.c_str());
  for (const auto& binding : clip_bindings) {
    std::fprintf(stdout, "  clip %s -> %s\n", binding.first.c_str(),
                 binding.second.c_str());
  }
  std::fprintf(stdout, "dogwalk_import_test_rig: scene assets/Scenes/dogwalk_test_rig.scene.asset\n");
  Blunder::g_runtime_global_context.m_logger_system.reset();
  return 0;
}
