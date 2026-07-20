#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

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

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_asset_compiler_freshness_test_" +
       std::to_string(
           static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Textures");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Textures");
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

// Minimal single-triangle glTF with embedded buffer (no external .bin).
constexpr char kMinimalTriangleGltf[] = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 1 },
      "indices": 0
    }]
  }],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5123,
      "count": 3,
      "type": "SCALAR"
    },
    {
      "bufferView": 1,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "max": [1.0, 1.0, 0.0],
      "min": [0.0, 0.0, 0.0]
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 6 },
    { "buffer": 0, "byteOffset": 8, "byteLength": 36 }
  ],
  "buffers": [{
    "byteLength": 44,
    "uri": "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/"
  }]
}
)";

void markFinalStaleDeletesMeshCookedArtifacts() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  const fs::path cooked = cookedMeshPath(file_system, eastl::string(kGuid));
  const fs::path meta = cookedMeshMetaPath(file_system, eastl::string(kGuid));
  writeBinaryFile(cooked, "MESH", 4);
  writeTextFile(meta, "source_mtime: 1\ndescriptor_mtime: 2\n");
  expect_true("precondition mesh cooked exists", file_system.exists(cooked));
  expect_true("precondition mesh meta exists", file_system.exists(meta));

  compiler.markFinalStale(eastl::string(kGuid));

  expect_true("markFinalStale removes meshbin", !file_system.exists(cooked));
  expect_true("markFinalStale removes meshbin.meta", !file_system.exists(meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void markFinalStaleDeletesTextureCookedArtifacts() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff";

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  const fs::path cooked = cookedTexturePath(file_system, eastl::string(kGuid));
  const fs::path meta = cookedTextureMetaPath(file_system, eastl::string(kGuid));
  writeBinaryFile(cooked, "TEXB", 4);
  writeTextFile(meta, "source_mtime: 3\ndescriptor_mtime: 4\n");
  expect_true("precondition tex cooked exists", file_system.exists(cooked));
  expect_true("precondition tex meta exists", file_system.exists(meta));

  compiler.markFinalStale(eastl::string(kGuid));

  expect_true("markFinalStale removes texbin", !file_system.exists(cooked));
  expect_true("markFinalStale removes texbin.meta", !file_system.exists(meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void cookAssetCooksRegisteredMeshAndSkipsWhenFresh() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "cccccccc-dddd-4eee-8fff-000000000001";
  const char* kDescriptorPath = "assets/Meshes/tri.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "tri.gltf",
                kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "tri.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/tri.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh for cookAsset",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  expect_true("cookAsset cooks mesh first time",
              compiler.cookAsset(eastl::string(kGuid)));
  expect_true(
      "cooked meshbin exists after cookAsset",
      file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));
  expect_true(
      "cooked mesh meta exists after cookAsset",
      file_system.exists(cookedMeshMetaPath(file_system, eastl::string(kGuid))));

  expect_true("cookAsset skips when Final is fresh",
              !compiler.cookAsset(eastl::string(kGuid)));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void markFinalStaleForcesRecook() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "dddddddd-eeee-4fff-8000-111111111111";
  const char* kDescriptorPath = "assets/Meshes/tri2.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "tri2.gltf",
                kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "tri2.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/tri2.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh for recook",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  expect_true("initial cook succeeds",
              compiler.cookAsset(eastl::string(kGuid)));
  expect_true("fresh cook skips", !compiler.cookAsset(eastl::string(kGuid)));

  compiler.markFinalStale(eastl::string(kGuid));
  expect_true(
      "Final gone after markFinalStale",
      !file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));

  expect_true("cookAsset after markFinalStale cooks again",
              compiler.cookAsset(eastl::string(kGuid)));
  expect_true(
      "Final restored after recook",
      file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));

  // Stub must be callable; dependency graph (4.x) owns real fan-out.
  compiler.cookDependents(eastl::string(kGuid));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void cookAssetForceRecooksFreshFinal() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "eeeeeeee-ffff-4000-8000-222222222222";
  const char* kDescriptorPath = "assets/Meshes/tri3.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "tri3.gltf",
                kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "tri3.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/tri3.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh for force cook",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  expect_true("cook once", compiler.cookAsset(eastl::string(kGuid)));
  expect_true("force=true recooks fresh Final",
              compiler.cookAsset(eastl::string(kGuid), true));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  markFinalStaleDeletesMeshCookedArtifacts();
  markFinalStaleDeletesTextureCookedArtifacts();
  cookAssetCooksRegisteredMeshAndSkipsWhenFresh();
  markFinalStaleForcesRecook();
  cookAssetForceRecooksFreshFinal();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "asset_compiler_freshness_test: all passed\n");
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
