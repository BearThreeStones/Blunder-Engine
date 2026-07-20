#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/asset_watch_path.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_dependency/asset_dependency_graph.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_manager/asset_manager.h"
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

void writeBinaryFile(const fs::path& path, const char* bytes, size_t size) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes, static_cast<std::streamsize>(size));
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
      "intermediate dae is IntermediateResource",
      classifyAssetWatchPath(resources / "Models" / "cube" / "cube.dae",
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
                    "source: resources/Models/cube/cube.dae\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n"
                    "texture_guids:\n"
                    "  - " +
                    kTexGuid + "\n");
  writeTextFile(project / "Resources" / "Models" / "cube" / "cube.dae",
                "dae");

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
        resources / "Models" / "cube" / "cube.dae", assets, resources,
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

void archivedSourcePathToGuids() {
  using namespace Blunder;
  ensureLogger();

  const char* kMeshGuid = "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
  const char* kOtherGuid = "dddddddd-dddd-4ddd-8ddd-dddddddddddd";

  const fs::path project = makeTempProject();
  writeTextFile(project / "Assets" / "Meshes" / "hero.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
                    "source: resources/Models/hero/hero.dae\n" +
                    "archived_source: Source/Models/hero.fbx\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n");
  writeTextFile(project / "Assets" / "Meshes" / "prop.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kOtherGuid + "\n" +
                    "source: resources/Models/prop/prop.dae\n" +
                    "archived_source: Source/Models/prop.fbx\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n");
  writeTextFile(project / "Resources" / "Models" / "hero" / "hero.dae",
                "dae");
  writeTextFile(project / "Resources" / "Models" / "prop" / "prop.dae",
                "dae");
  writeTextFile(project / "Resources" / "Source" / "Models" / "hero.fbx",
                "fbx");
  writeTextFile(project / "Resources" / "Source" / "Models" / "prop.fbx",
                "fbx");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  const fs::path resources = file_system.getResourcesRoot();
  const fs::path hero_source = resources / "Source" / "Models" / "hero.fbx";

  {
    const eastl::vector<eastl::string> guids =
        guidsForArchivedSourcePath(hero_source, resources, registry,
                                   file_system);
    expect_true("archived Source path maps to owning mesh guid",
                containsGuid(guids, kMeshGuid));
    expect_true("archived Source path does not include unrelated guid",
                !containsGuid(guids, kOtherGuid));
  }

  {
    const eastl::vector<eastl::string> guids = guidsForArchivedSourcePath(
        resources / "Source" / "Models" / "missing.fbx", resources, registry,
        file_system);
    expect_true("unknown archived Source yields no guids", guids.empty());
  }

  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void intermediateTextureChangeInvalidatesMeshFinal() {
  using namespace Blunder;
  ensureLogger();

  const char* kMeshGuid = "11111111-1111-4111-8111-111111111111";
  const char* kTexGuid = "22222222-2222-4222-8222-222222222222";

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
                    "source: resources/Models/cube/cube.dae\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n"
                    "texture_guids:\n"
                    "  - " +
                    kTexGuid + "\n");
  writeTextFile(project / "Resources" / "Models" / "cube" / "cube.dae",
                "dae");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);
  compiler.rebuildDependencyGraph();

  AssetDependencyGraph graph;
  graph.rebuildFromProject(file_system, registry);

  const fs::path assets = file_system.getAssetRoot();
  const fs::path resources = file_system.getResourcesRoot();
  const fs::path mesh_cooked =
      cookedMeshPath(file_system, eastl::string(kMeshGuid));
  const fs::path mesh_meta =
      cookedMeshMetaPath(file_system, eastl::string(kMeshGuid));
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
      AssetWatchPathClass::IntermediateResource,
      resources / "Textures" / "albedo.png", assets, resources, registry,
      graph);
  expect_true("Intermediate texture maps to texture guid",
              containsGuid(guids, kTexGuid));

  for (const eastl::string& guid : guids) {
    compiler.invalidateAssetAndDependents(guid);
  }

  expect_true("texture Intermediate change invalidates dependent mesh Final",
              !file_system.exists(mesh_cooked));
  expect_true("texture Intermediate change invalidates dependent mesh meta",
              !file_system.exists(mesh_meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void descriptorChangeInvalidatesFinal() {
  using namespace Blunder;
  ensureLogger();

  const char* kMeshGuid = "33333333-3333-4333-8333-333333333333";

  const fs::path project = makeTempProject();
  writeTextFile(project / "Assets" / "Meshes" / "solo.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
                    "source: resources/Models/solo/solo.dae\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n");
  writeTextFile(project / "Resources" / "Models" / "solo" / "solo.dae",
                "dae");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);
  compiler.rebuildDependencyGraph();

  AssetDependencyGraph graph;
  graph.rebuildFromProject(file_system, registry);

  const fs::path assets = file_system.getAssetRoot();
  const fs::path resources = file_system.getResourcesRoot();
  const fs::path mesh_cooked =
      cookedMeshPath(file_system, eastl::string(kMeshGuid));
  const fs::path mesh_meta =
      cookedMeshMetaPath(file_system, eastl::string(kMeshGuid));
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
      AssetWatchPathClass::AssetsTree,
      assets / "Meshes" / "solo.mesh.yaml", assets, resources, registry,
      graph);
  expect_true("descriptor path maps to mesh guid",
              containsGuid(guids, kMeshGuid));

  for (const eastl::string& guid : guids) {
    compiler.invalidateAssetAndDependents(guid);
  }

  expect_true("descriptor change invalidates own Final",
              !file_system.exists(mesh_cooked));
  expect_true("descriptor change invalidates own meta",
              !file_system.exists(mesh_meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void sourceChangeTriggersReimportInvalidatesFinal() {
  using namespace Blunder;
  ensureLogger();

  const char* kMeshGuid = "44444444-4444-4444-8444-444444444444";

  const fs::path project = makeTempProject();
  writeTextFile(project / "Assets" / "Meshes" / "hero.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
                    "source: resources/Models/hero/hero.dae\n" +
                    "archived_source: Source/Models/hero.fbx\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n");
  writeTextFile(project / "Resources" / "Models" / "hero" / "hero.dae",
                "dae");
  writeTextFile(project / "Resources" / "Source" / "Models" / "hero.fbx",
                "fbx");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  const fs::path resources = file_system.getResourcesRoot();
  const fs::path hero_source = resources / "Source" / "Models" / "hero.fbx";
  const fs::path mesh_cooked =
      cookedMeshPath(file_system, eastl::string(kMeshGuid));
  const fs::path mesh_meta =
      cookedMeshMetaPath(file_system, eastl::string(kMeshGuid));
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  const eastl::vector<eastl::string> guids =
      import_service.findGuidsByArchivedSource(hero_source);
  expect_true("Source change maps to owning mesh guid via archived_source",
              containsGuid(guids, kMeshGuid));

  expect_true("Reimport hook succeeds for Source-mapped guid",
              import_service.requestReimports(guids));
  expect_true("Source Reimport hook invalidates mesh Final",
              !file_system.exists(mesh_cooked));
  expect_true("Source Reimport hook invalidates mesh meta",
              !file_system.exists(mesh_meta));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

void reimportBatchRebuildsGraphOnce() {
  using namespace Blunder;
  ensureLogger();

  const char* kMeshGuid = "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
  const char* kOtherGuid = "ffffffff-ffff-4fff-8fff-ffffffffffff";

  const fs::path project = makeTempProject();
  writeTextFile(project / "Assets" / "Meshes" / "hero.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kMeshGuid + "\n" +
                    "source: resources/Models/hero/hero.dae\n" +
                    "archived_source: Source/Models/hero.fbx\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n");
  writeTextFile(project / "Assets" / "Meshes" / "prop.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kOtherGuid + "\n" +
                    "source: resources/Models/prop/prop.dae\n" +
                    "archived_source: Source/Models/prop.fbx\n" +
                    "import:\n"
                    "  generate_normals: true\n"
                    "  generate_tangents: true\n"
                    "  scale: 1.0\n");
  writeTextFile(project / "Resources" / "Models" / "hero" / "hero.dae",
                "dae");
  writeTextFile(project / "Resources" / "Models" / "prop" / "prop.dae",
                "dae");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  eastl::vector<eastl::string> guids;
  guids.push_back(kMeshGuid);
  guids.push_back(kOtherGuid);

  const uint32_t before = compiler.dependencyGraphRebuildCount();
  expect_true("requestReimports succeeds for two guids",
              import_service.requestReimports(guids));
  const uint32_t after = compiler.dependencyGraphRebuildCount();
  expect_true("batch reimport rebuilds dependency graph once (not per GUID)",
              after == before + 1);

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

}  // namespace

int main() {
  classifyPaths();
  pathToGuidMapping();
  archivedSourcePathToGuids();
  intermediateTextureChangeInvalidatesMeshFinal();
  descriptorChangeInvalidatesFinal();
  sourceChangeTriggersReimportInvalidatesFinal();
  reimportBatchRebuildsGraphOnce();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("asset_watch_path_test: all passed\n");
  return 0;
}
