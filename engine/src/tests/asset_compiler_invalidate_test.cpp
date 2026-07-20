#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/asset_watch_path.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_dependency/asset_dependency_graph.h"
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
      ("blunder_asset_compiler_invalidate_test_" +
       std::to_string(
           static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Textures");
  fs::create_directories(root / "Assets" / "Scenes");
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

struct GraphFixture {
  fs::path project;
  const char* scene_guid = "11111111-1111-4111-8111-111111111111";
  const char* mesh_guid = "22222222-2222-4222-8222-222222222222";
  const char* tex_guid = "33333333-3333-4333-8333-333333333333";
};

GraphFixture writeTextureMeshSceneFixture() {
  GraphFixture fixture;
  fixture.project = makeTempProject();

  writeTextFile(
      fixture.project / "Assets" / "Textures" / "albedo.texture.yaml",
      std::string("type: Texture2D\n") + "guid: " + fixture.tex_guid + "\n" +
          "source: resources/Textures/albedo.png\n" +
          "import:\n"
          "  srgb: true\n"
          "  generate_mips: false\n");
  writeTextFile(fixture.project / "Resources" / "Textures" / "albedo.png",
                "png");

  writeTextFile(
      fixture.project / "Assets" / "Meshes" / "hero.mesh.yaml",
      std::string("type: Mesh\n") + "guid: " + fixture.mesh_guid + "\n" +
          "source: resources/Models/hero.gltf\n" + "texture_guids:\n" +
          "  - " + fixture.tex_guid + "\n" +
          "import:\n"
          "  materials: false\n"
          "  animations: false\n"
          "  scale: 1.0\n");
  writeTextFile(fixture.project / "Resources" / "Models" / "hero.gltf",
                kMinimalTriangleGltf);

  writeTextFile(
      fixture.project / "Assets" / "Scenes" / "level.scene.asset",
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
          fixture.scene_guid + "\",\n" + "  \"entities\": [\n" + "    {\n" +
          "      \"name\": \"Hero\",\n" +
          "      \"position\": [0, 0, 0],\n" +
          "      \"rotation\": [0, 0, 0],\n" +
          "      \"rotationMode\": \"euler_degrees\",\n" +
          "      \"mesh\": \"" + fixture.mesh_guid + "\"\n" + "    }\n" +
          "  ]\n" + "}\n");

  return fixture;
}

void invalidateMarksSelfAndDependents() {
  using namespace Blunder;
  ensureLogger();

  const GraphFixture fixture = writeTextureMeshSceneFixture();

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = fixture.project;
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

  const fs::path tex_cooked =
      cookedTexturePath(file_system, eastl::string(fixture.tex_guid));
  const fs::path tex_meta =
      cookedTextureMetaPath(file_system, eastl::string(fixture.tex_guid));
  const fs::path mesh_cooked =
      cookedMeshPath(file_system, eastl::string(fixture.mesh_guid));
  const fs::path mesh_meta =
      cookedMeshMetaPath(file_system, eastl::string(fixture.mesh_guid));

  writeBinaryFile(tex_cooked, "TEXB", 4);
  writeTextFile(tex_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 3\ndescriptor_mtime: 4\n");

  expect_true("precondition tex Final exists", file_system.exists(tex_cooked));
  expect_true("precondition mesh Final exists",
              file_system.exists(mesh_cooked));

  compiler.invalidateAssetAndDependents(eastl::string(fixture.tex_guid));

  expect_true("invalidate removes texture Final",
              !file_system.exists(tex_cooked));
  expect_true("invalidate removes texture meta", !file_system.exists(tex_meta));
  expect_true("invalidate removes dependent mesh Final",
              !file_system.exists(mesh_cooked));
  expect_true("invalidate removes dependent mesh meta",
              !file_system.exists(mesh_meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(fixture.project);
}

void invalidateWithoutDependentsOnlyMarksSelf() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";

  writeTextFile(
      project / "Assets" / "Textures" / "solo.texture.yaml",
      std::string("type: Texture2D\n") + "guid: " + kGuid + "\n" +
          "source: resources/Textures/solo.png\n" +
          "import:\n"
          "  srgb: true\n"
          "  generate_mips: false\n");
  writeTextFile(project / "Resources" / "Textures" / "solo.png", "png");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
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

  const fs::path cooked =
      cookedTexturePath(file_system, eastl::string(kGuid));
  const fs::path meta =
      cookedTextureMetaPath(file_system, eastl::string(kGuid));
  writeBinaryFile(cooked, "TEXB", 4);
  writeTextFile(meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  compiler.invalidateAssetAndDependents(eastl::string(kGuid));

  expect_true("solo invalidate removes own Final", !file_system.exists(cooked));
  expect_true("solo invalidate removes own meta", !file_system.exists(meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void cookDependentsRecooksMeshAfterTextureInvalidate() {
  using namespace Blunder;
  ensureLogger();

  const GraphFixture fixture = writeTextureMeshSceneFixture();

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = fixture.project;
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

  expect_true("initial mesh cook succeeds",
              compiler.cookAsset(eastl::string(fixture.mesh_guid)));

  const fs::path mesh_cooked =
      cookedMeshPath(file_system, eastl::string(fixture.mesh_guid));
  expect_true("mesh Final present after cook",
              file_system.exists(mesh_cooked));

  compiler.invalidateAssetAndDependents(eastl::string(fixture.tex_guid));
  expect_true("mesh Final gone after texture invalidate",
              !file_system.exists(mesh_cooked));

  compiler.cookDependents(eastl::string(fixture.tex_guid));
  expect_true("cookDependents restores mesh Final",
              file_system.exists(mesh_cooked));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(fixture.project);
}

// Task 4.5 theme 1: Intermediate texture body change → map GUID → invalidate
// dependents → mesh Final stale.
void textureIntermediateChangeInvalidatesMeshFinal() {
  using namespace Blunder;
  ensureLogger();

  const GraphFixture fixture = writeTextureMeshSceneFixture();

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = fixture.project;
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

  const fs::path mesh_cooked =
      cookedMeshPath(file_system, eastl::string(fixture.mesh_guid));
  const fs::path mesh_meta =
      cookedMeshMetaPath(file_system, eastl::string(fixture.mesh_guid));
  const fs::path tex_cooked =
      cookedTexturePath(file_system, eastl::string(fixture.tex_guid));
  writeBinaryFile(tex_cooked, "TEXB", 4);
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 3\ndescriptor_mtime: 4\n");

  const fs::path assets = file_system.getAssetRoot();
  const fs::path resources = file_system.getResourcesRoot();
  const fs::path intermediate_tex = resources / "Textures" / "albedo.png";

  const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
      AssetWatchPathClass::IntermediateResource, intermediate_tex, assets,
      resources, registry, graph);
  expect_true("intermediate texture maps to texture guid",
              !guids.empty() && guids[0] == fixture.tex_guid);

  for (const eastl::string& guid : guids) {
    compiler.invalidateAssetAndDependents(guid);
  }

  expect_true("texture Intermediate change removes dependent mesh Final",
              !file_system.exists(mesh_cooked));
  expect_true("texture Intermediate change removes dependent mesh meta",
              !file_system.exists(mesh_meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(fixture.project);
}

// Task 4.5 theme 2: descriptor change → map GUID → invalidate → Final stale.
void descriptorChangeInvalidatesOwnFinal() {
  using namespace Blunder;
  ensureLogger();

  const GraphFixture fixture = writeTextureMeshSceneFixture();

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = fixture.project;
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

  const fs::path tex_cooked =
      cookedTexturePath(file_system, eastl::string(fixture.tex_guid));
  const fs::path tex_meta =
      cookedTextureMetaPath(file_system, eastl::string(fixture.tex_guid));
  writeBinaryFile(tex_cooked, "TEXB", 4);
  writeTextFile(tex_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  const fs::path assets = file_system.getAssetRoot();
  const fs::path resources = file_system.getResourcesRoot();
  const fs::path descriptor =
      assets / "Textures" / "albedo.texture.yaml";

  const eastl::vector<eastl::string> guids = guidsToInvalidateForWatchedPath(
      AssetWatchPathClass::AssetsTree, descriptor, assets, resources, registry,
      graph);
  expect_true("descriptor path maps to texture guid",
              !guids.empty() && guids[0] == fixture.tex_guid);

  for (const eastl::string& guid : guids) {
    compiler.invalidateAssetAndDependents(guid);
  }

  expect_true("descriptor change removes own Final",
              !file_system.exists(tex_cooked));
  expect_true("descriptor change removes own meta",
              !file_system.exists(tex_meta));

  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(fixture.project);
}

}  // namespace

int main() {
  invalidateMarksSelfAndDependents();
  invalidateWithoutDependentsOnlyMarksSelf();
  cookDependentsRecooksMeshAfterTextureInvalidate();
  textureIntermediateChangeInvalidatesMeshFinal();
  descriptorChangeInvalidatesOwnFinal();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "asset_compiler_invalidate_test: all passed\n");
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
