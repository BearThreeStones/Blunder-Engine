#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_dependency/asset_dependency_graph.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
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

bool containsGuid(const eastl::vector<eastl::string>& list,
                  const char* guid) {
  return std::find_if(list.begin(), list.end(),
                      [guid](const eastl::string& value) {
                        return value == guid;
                      }) != list.end();
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_asset_dependency_graph_test_" +
       std::to_string(
           static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Animations");
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Textures");
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / "Resources" / "Animations");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Textures");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

void rebuildSceneMeshTextureEdgesAndLeaves() {
  using namespace Blunder;
  ensureLogger();

  const char* kSceneGuid = "11111111-1111-4111-8111-111111111111";
  const char* kMeshGuid = "22222222-2222-4222-8222-222222222222";
  const char* kTexGuid = "33333333-3333-4333-8333-333333333333";

  const fs::path project = makeTempProject();
  writeTextFile(
      project / "Assets" / "Textures" / "albedo.texture.yaml",
      std::string("type: Texture2D\n") + "guid: " + kTexGuid + "\n" +
          "source: Resources/Textures/albedo.png\n" +
          "import:\n"
          "  srgb: true\n"
          "  generate_mips: false\n");
  writeTextFile(project / "Resources" / "Textures" / "albedo.png", "png");

  writeTextFile(
      project / "Assets" / "Meshes" / "hero.mesh.yaml",
      std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
          "source: Resources/Models/hero.gltf\n" + "texture_guids:\n" +
          "  - " + kTexGuid + "\n" +
          "import:\n"
          "  materials: true\n"
          "  animations: false\n"
          "  scale: 1.0\n");
  writeTextFile(project / "Resources" / "Models" / "hero.gltf", "{}");

  writeTextFile(
      project / "Assets" / "Scenes" / "level.scene.asset",
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
          kSceneGuid + "\",\n" + "  \"entities\": [\n" + "    {\n" +
          "      \"name\": \"Hero\",\n" +
          "      \"position\": [0, 0, 0],\n" +
          "      \"rotation\": [0, 0, 0],\n" +
          "      \"rotationMode\": \"euler_degrees\",\n" +
          "      \"mesh\": \"" + kMeshGuid + "\"\n" + "    }\n" +
          "  ]\n" + "}\n");

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetDependencyGraph graph;
  graph.rebuildFromProject(file_system, registry);

  const eastl::vector<eastl::string> mesh_deps = graph.dependentsOf(kMeshGuid);
  expect_true("mesh dependents include scene",
              containsGuid(mesh_deps, kSceneGuid));
  expect_true("mesh dependents size 1", mesh_deps.size() == 1);

  const eastl::vector<eastl::string> tex_deps = graph.dependentsOf(kTexGuid);
  expect_true("texture dependents include mesh",
              containsGuid(tex_deps, kMeshGuid));
  expect_true("texture dependents size 1", tex_deps.size() == 1);

  expect_true("scene has no dependents",
              graph.dependentsOf(kSceneGuid).empty());

  const AssetDependencyLeaves mesh_leaves =
      graph.intermediateLeavesOf(kMeshGuid);
  expect_true("mesh leaf descriptor path",
              mesh_leaves.descriptor_virtual_path ==
                  "assets/Meshes/hero.mesh.yaml");
  expect_true("mesh leaf intermediate source",
              mesh_leaves.intermediate_source_path ==
                  "Resources/Models/hero.gltf");

  const AssetDependencyLeaves tex_leaves =
      graph.intermediateLeavesOf(kTexGuid);
  expect_true("texture leaf descriptor path",
              tex_leaves.descriptor_virtual_path ==
                  "assets/Textures/albedo.texture.yaml");
  expect_true("texture leaf intermediate source",
              tex_leaves.intermediate_source_path ==
                  "Resources/Textures/albedo.png");

  const AssetDependencyLeaves scene_leaves =
      graph.intermediateLeavesOf(kSceneGuid);
  expect_true("scene leaf descriptor path",
              scene_leaves.descriptor_virtual_path ==
                  "assets/Scenes/level.scene.asset");
  expect_true("scene leaf intermediate source empty",
              scene_leaves.intermediate_source_path.empty());

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void rebuildClearsPreviousEdges() {
  using namespace Blunder;
  ensureLogger();

  const char* kSceneGuid = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
  const char* kMeshGuid = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";

  const fs::path project = makeTempProject();
  writeTextFile(
      project / "Assets" / "Meshes" / "cube.mesh.yaml",
      std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
          "source: Resources/Models/cube.gltf\n" +
          "import:\n"
          "  materials: true\n"
          "  animations: false\n"
          "  scale: 1.0\n");
  writeTextFile(project / "Resources" / "Models" / "cube.gltf", "{}");
  writeTextFile(
      project / "Assets" / "Scenes" / "a.scene.asset",
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
          kSceneGuid + "\",\n" + "  \"entities\": [\n" + "    {\n" +
          "      \"name\": \"Cube\",\n" +
          "      \"position\": [0, 0, 0],\n" +
          "      \"rotation\": [0, 0, 0],\n" +
          "      \"rotationMode\": \"euler_degrees\",\n" +
          "      \"mesh\": \"" + kMeshGuid + "\"\n" + "    }\n" +
          "  ]\n" + "}\n");

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetDependencyGraph graph;
  graph.rebuildFromProject(file_system, registry);
  expect_true("first rebuild has scene dependent",
              containsGuid(graph.dependentsOf(kMeshGuid), kSceneGuid));

  fs::remove(project / "Assets" / "Scenes" / "a.scene.asset");
  registry.rebuildFromScan();
  graph.rebuildFromProject(file_system, registry);
  expect_true("rebuild clears removed scene edge",
              graph.dependentsOf(kMeshGuid).empty());

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void rebuildAnimationClipLeavesAndSceneEdge() {
  using namespace Blunder;
  ensureLogger();

  const char* kSceneGuid = "12121212-1212-4212-8212-121212121212";
  const char* kClipGuid = "23232323-2323-4232-8232-232323232323";

  const fs::path project = makeTempProject();
  writeTextFile(
      project / "Assets" / "Animations" / "walk.animation.yaml",
      std::string("type: AnimationClip\n") + "guid: " + kClipGuid + "\n" +
          "source: resources/Animations/walk.anim.yaml\n");
  writeTextFile(
      project / "Resources" / "Animations" / "walk.anim.yaml",
      std::string("version: 1\n") + "name: walk\n" + "duration: 1.0\n" +
          "tracks:\n" +
          "  - bone: Hips\n" +
          "    channel: translation\n" +
          "    interpolation: Linear\n" +
          "    keys:\n" +
          "      - time: 0.0\n" +
          "        value: [0, 0, 0]\n" +
          "      - time: 1.0\n" +
          "        value: [0, 1, 0]\n");

  writeTextFile(
      project / "Assets" / "Scenes" / "anim.scene.asset",
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
          kSceneGuid + "\",\n" + "  \"entities\": [\n" + "    {\n" +
          "      \"name\": \"Dog\",\n" +
          "      \"position\": [0, 0, 0],\n" +
          "      \"rotation\": [0, 0, 0],\n" +
          "      \"rotationMode\": \"euler_degrees\",\n" +
          "      \"animation_clip_guids\": [\n" +
          "        \"" + kClipGuid + "\"\n" + "      ]\n" + "    }\n" +
          "  ]\n" + "}\n");

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetDependencyGraph graph;
  graph.rebuildFromProject(file_system, registry);

  const eastl::vector<eastl::string> clip_deps = graph.dependentsOf(kClipGuid);
  expect_true("clip dependents include scene",
              containsGuid(clip_deps, kSceneGuid));
  expect_true("clip dependents size 1", clip_deps.size() == 1);

  const AssetDependencyLeaves clip_leaves = graph.intermediateLeavesOf(kClipGuid);
  expect_true("clip leaf descriptor path",
              clip_leaves.descriptor_virtual_path ==
                  "assets/Animations/walk.animation.yaml");
  expect_true("clip leaf intermediate source",
              clip_leaves.intermediate_source_path ==
                  "resources/Animations/walk.anim.yaml");

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void ignoresLegacyPathMeshRefs() {
  using namespace Blunder;
  ensureLogger();

  const char* kSceneGuid = "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
  const char* kMeshGuid = "dddddddd-dddd-4ddd-8ddd-dddddddddddd";

  const fs::path project = makeTempProject();
  writeTextFile(
      project / "Assets" / "Meshes" / "cube.mesh.yaml",
      std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
          "source: Resources/Models/cube.gltf\n" +
          "import:\n"
          "  materials: true\n"
          "  animations: false\n"
          "  scale: 1.0\n");
  writeTextFile(
      project / "Assets" / "Scenes" / "legacy.scene.asset",
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
          kSceneGuid + "\",\n" + "  \"entities\": [\n" + "    {\n" +
          "      \"name\": \"Cube\",\n" +
          "      \"position\": [0, 0, 0],\n" +
          "      \"rotation\": [0, 0, 0],\n" +
          "      \"rotationMode\": \"euler_degrees\",\n" +
          "      \"mesh\": \"assets/Meshes/cube.mesh.yaml\"\n" + "    }\n" +
          "  ]\n" + "}\n");

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetDependencyGraph graph;
  graph.rebuildFromProject(file_system, registry);
  expect_true("legacy path mesh does not create edge",
              graph.dependentsOf(kMeshGuid).empty());

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  rebuildSceneMeshTextureEdgesAndLeaves();
  rebuildAnimationClipLeavesAndSceneEdge();
  rebuildClearsPreviousEdges();
  ignoresLegacyPathMeshRefs();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "asset_dependency_graph_test: all passed\n");
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
