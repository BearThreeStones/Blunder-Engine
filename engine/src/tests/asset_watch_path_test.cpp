#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_dependency/asset_dependency_graph.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/resource/asset_cook/asset_watch_path.h"

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

bool containsGuid(const eastl::vector<eastl::string>& list, const char* guid) {
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
      ("blunder_asset_watch_path_test_" +
       std::to_string(
           static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Textures");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Textures");
  fs::create_directories(root / "Resources" / "Source" / "Models");
  fs::create_directories(root / ".blunder" / "cooked");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

void classifyPaths() {
  using namespace Blunder;
  const fs::path project = makeTempProject();
  const fs::path assets = project / "Assets";
  const fs::path resources = project / "Resources";

  expect_true(
      "assets descriptor is AssetsTree",
      classifyAssetWatchPath(assets / "Meshes" / "cube.mesh.yaml", assets,
                             resources) == AssetWatchPathClass::AssetsTree);

  expect_true(
      "intermediate gltf is IntermediateResource",
      classifyAssetWatchPath(resources / "Models" / "cube" / "cube.gltf",
                             assets, resources) ==
          AssetWatchPathClass::IntermediateResource);

  expect_true(
      "Resources/Source is SourceArchive (not Intermediate invalidate)",
      classifyAssetWatchPath(resources / "Source" / "Models" / "cube.fbx",
                             assets, resources) ==
          AssetWatchPathClass::SourceArchive);

  expect_true(
      "outside roots is Ignored",
      classifyAssetWatchPath(project / ".blunder" / "cooked" / "x.meshbin",
                             assets, resources) == AssetWatchPathClass::Ignored);

  expect_true(
      ".blunder token under Assets is Ignored",
      classifyAssetWatchPath(assets / ".blunder" / "tmp", assets, resources) ==
          AssetWatchPathClass::Ignored);
}

void pathToGuidMapping() {
  using namespace Blunder;
  ensureLogger();

  const char* kMeshGuid = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
  const char* kTexGuid = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";

  const fs::path project = makeTempProject();
  writeTextFile(project / "Assets" / "Textures" / "albedo.texture.yaml",
                std::string("type: Texture2D\n") + "guid: " + kTexGuid + "\n" +
                    "source: resources/Textures/albedo.png\n" +
                    "import:\n"
                    "  srgb: true\n"
                    "  generate_mips: false\n");
  writeTextFile(project / "Resources" / "Textures" / "albedo.png", "png");
  writeTextFile(project / "Assets" / "Meshes" / "cube.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
                    "source: resources/Models/cube/cube.gltf\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n"
                    "texture_guids:\n"
                    "  - " +
                    kTexGuid + "\n");
  writeTextFile(project / "Resources" / "Models" / "cube" / "cube.gltf",
                "gltf");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetDependencyGraph graph;
  graph.rebuildFromProject(file_system, registry);

  const fs::path assets = file_system.getAssetRoot();
  const fs::path resources = file_system.getResourcesRoot();

  {
    const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
        AssetWatchPathClass::AssetsTree,
        assets / "Textures" / "albedo.texture.yaml", assets, resources,
        registry, graph);
    expect_true("descriptor path maps to texture guid",
                containsGuid(guids, kTexGuid));
    expect_true("descriptor path does not invent mesh guid",
                !containsGuid(guids, kMeshGuid));
  }

  {
    const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
        AssetWatchPathClass::IntermediateResource,
        resources / "Textures" / "albedo.png", assets, resources, registry,
        graph);
    expect_true("intermediate texture body maps to texture guid",
                containsGuid(guids, kTexGuid));
  }

  {
    const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
        AssetWatchPathClass::IntermediateResource,
        resources / "Models" / "cube" / "cube.gltf", assets, resources,
        registry, graph);
    expect_true("intermediate mesh body maps to mesh guid",
                containsGuid(guids, kMeshGuid));
  }

  {
    const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
        AssetWatchPathClass::SourceArchive,
        resources / "Source" / "Models" / "cube.fbx", assets, resources,
        registry, graph);
    expect_true("Source archive yields no Intermediate invalidate guids",
                guids.empty());
  }

  {
    const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
        AssetWatchPathClass::Ignored, project / ".blunder" / "cooked" / "x.bin",
        assets, resources, registry, graph);
    expect_true("Ignored path yields no guids", guids.empty());
  }

  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

}  // namespace

int main() {
  classifyPaths();
  pathToGuidMapping();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("asset_watch_path_test: all passed\n");
  return 0;
}
