#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_cook/asset_cook_types.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include "EASTL/shared_ptr.h"

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
      ("blunder_asset_manager_fast_path_test_" +
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

void writeBinaryFile(const fs::path& path, const unsigned char* bytes,
                     size_t size) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes),
            static_cast<std::streamsize>(size));
}

// Minimal single-triangle COLLADA Intermediate (Assimp-readable).
constexpr char kMinimalTriangleDae[] = R"(<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset>
    <up_axis>Y_UP</up_axis>
  </asset>
  <library_geometries>
    <geometry id="tri-mesh" name="tri">
      <mesh>
        <source id="tri-positions">
          <float_array id="tri-positions-array" count="9">0 0 0 1 0 0 0 1 0</float_array>
          <technique_common>
            <accessor source="#tri-positions-array" count="3" stride="3">
              <param name="X" type="float"/>
              <param name="Y" type="float"/>
              <param name="Z" type="float"/>
            </accessor>
          </technique_common>
        </source>
        <vertices id="tri-vertices">
          <input semantic="POSITION" source="#tri-positions"/>
        </vertices>
        <triangles count="1">
          <input semantic="VERTEX" source="#tri-vertices" offset="0"/>
          <p>0 1 2</p>
        </triangles>
      </mesh>
    </geometry>
  </library_geometries>
  <library_visual_scenes>
    <visual_scene id="Scene" name="Scene">
      <node id="tri-node" name="tri">
        <instance_geometry url="#tri-mesh"/>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene>
    <instance_visual_scene url="#Scene"/>
  </scene>
</COLLADA>
)";

// Legacy glTF Intermediate (cgltf Fast Path while source still points at glTF).
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

// 1x1 RGB PNG (red pixel).
constexpr unsigned char kMinimalPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
    0x00, 0x03, 0x01, 0x01, 0x00, 0xC9, 0xFE, 0x92, 0xEF, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

void loadMeshMissingFinalFastPathRequestsCook() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeee01";
  const char* kDescriptorPath = "assets/Meshes/tri.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "tri.dae",
                kMinimalTriangleDae);
  writeTextFile(project / "Assets" / "Meshes" / "tri.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/tri.dae\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh descriptor",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  manager.setAssetCompiler(compiler);

  expect_true(
      "precondition: Final missing",
      !file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("Fast Path load returns mesh", mesh != nullptr);
  expect_true("Fast Path mesh has vertices",
              mesh && mesh->getVertexCount() == 3);
  expect_true(
      "Fast Path load requested cook (Final written)",
      file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));
  expect_true(
      "Fast Path cook wrote meta",
      file_system.exists(cookedMeshMetaPath(file_system, eastl::string(kGuid))));

  manager.setAssetCompiler({});
  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void loadTextureMissingFinalFastPathRequestsCook() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "bbbbbbbb-cccc-4ddd-8eee-ffffffffff02";
  const char* kDescriptorPath = "assets/Textures/pixel.texture.yaml";

  writeBinaryFile(project / "Resources" / "Textures" / "pixel.png", kMinimalPng,
                  sizeof(kMinimalPng));
  writeTextFile(project / "Assets" / "Textures" / "pixel.texture.yaml",
                std::string("type: Texture2D\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Textures/pixel.png\n" +
                    "import:\n  srgb: true\n  generate_mips: false\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register texture descriptor",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  manager.setAssetCompiler(compiler);

  expect_true(
      "precondition: texture Final missing",
      !file_system.exists(cookedTexturePath(file_system, eastl::string(kGuid))));

  const eastl::shared_ptr<Texture2DAsset> texture =
      manager.loadTexture2D(eastl::string(kDescriptorPath));
  expect_true("Fast Path texture load returns asset", texture != nullptr);
  expect_true("Fast Path texture is 1x1",
              texture && texture->getWidth() == 1 && texture->getHeight() == 1);
  expect_true(
      "Fast Path texture load requested cook (Final written)",
      file_system.exists(cookedTexturePath(file_system, eastl::string(kGuid))));
  expect_true("Fast Path texture cook wrote meta",
              file_system.exists(
                  cookedTextureMetaPath(file_system, eastl::string(kGuid))));

  manager.setAssetCompiler({});
  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

bool pathContains(const fs::path& path, const char* needle) {
  const std::string s = path.generic_string();
  return s.find(needle) != std::string::npos;
}

void loadMeshStaleMetaFastPathRequestsCook() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "cccccccc-dddd-4eee-8fff-aaaaaaaaaa03";
  const char* kDescriptorPath = "assets/Meshes/stale.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "stale.dae",
                kMinimalTriangleDae);
  writeTextFile(project / "Assets" / "Meshes" / "stale.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/stale.dae\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh for stale meta",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  manager.setAssetCompiler(compiler);

  expect_true("precondition: initial cook writes Final",
              compiler->cookAsset(eastl::string(kGuid)));
  expect_true(
      "precondition: Final exists before stale meta",
      file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));

  CookedAssetMeta stale_meta{};
  stale_meta.source_mtime = 1;
  stale_meta.descriptor_mtime = 1;
  expect_true(
      "precondition: write stale meta",
      writeCookMetaFile(cookedMeshMetaPath(file_system, eastl::string(kGuid)),
                        stale_meta));

  manager.clearCache();

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("stale meta Fast Path returns mesh", mesh != nullptr);
  expect_true("stale meta Fast Path uses Intermediate COLLADA",
              mesh && pathContains(mesh->getAbsolutePath(), ".dae"));
  expect_true("stale meta Fast Path does not use descriptor as absolute",
              mesh && !pathContains(mesh->getAbsolutePath(), ".mesh.yaml"));

  CookedAssetMeta refreshed{};
  expect_true(
      "stale meta Fast Path requested cook (meta readable)",
      readCookMetaFile(cookedMeshMetaPath(file_system, eastl::string(kGuid)),
                       refreshed));
  expect_true("stale meta Fast Path cook refreshed meta",
              refreshed.source_mtime != 1 || refreshed.descriptor_mtime != 1);

  manager.setAssetCompiler({});
  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void loadMeshFreshFinalPreferred() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "dddddddd-eeee-4fff-8000-bbbbbbbbbb04";
  const char* kDescriptorPath = "assets/Meshes/fresh.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "fresh.dae",
                kMinimalTriangleDae);
  writeTextFile(project / "Assets" / "Meshes" / "fresh.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/fresh.dae\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register mesh for fresh Final",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);

  expect_true("precondition: cook fresh Final",
              compiler->cookAsset(eastl::string(kGuid)));
  expect_true(
      "precondition: fresh Final bin exists",
      file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));
  expect_true(
      "precondition: fresh Final meta exists",
      file_system.exists(cookedMeshMetaPath(file_system, eastl::string(kGuid))));

  // No compiler on manager: if load wrongly took Fast Path it would still
  // return Intermediate, but must not need a cook request to succeed.
  manager.clearCache();

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("fresh Final preferred returns mesh", mesh != nullptr);
  expect_true("fresh Final preferred uses descriptor absolute path",
              mesh && pathContains(mesh->getAbsolutePath(), ".mesh.yaml"));
  expect_true("fresh Final preferred does not use Intermediate COLLADA",
              mesh && !pathContains(mesh->getAbsolutePath(), ".dae"));
  expect_true("fresh Final preferred has vertices",
              mesh && mesh->getVertexCount() == 3);

  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void loadMeshLegacyGltfFastPathStillWorks() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "eeeeeeee-ffff-4000-8000-cccccccccc05";
  const char* kDescriptorPath = "assets/Meshes/legacy.mesh.yaml";

  writeTextFile(project / "Resources" / "Models" / "legacy.gltf",
                kMinimalTriangleGltf);
  writeTextFile(project / "Assets" / "Meshes" / "legacy.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy.gltf\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  expect_true("register legacy glTF mesh",
              registry.registerAsset(eastl::string(kGuid),
                                     eastl::string(kDescriptorPath)));

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  manager.setAssetCompiler(compiler);

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string(kDescriptorPath));
  expect_true("legacy glTF Fast Path returns mesh", mesh != nullptr);
  expect_true("legacy glTF Fast Path has vertices",
              mesh && mesh->getVertexCount() == 3);
  expect_true("legacy glTF Fast Path uses Intermediate glTF",
              mesh && pathContains(mesh->getAbsolutePath(), ".gltf"));
  expect_true(
      "legacy glTF Fast Path requested cook",
      file_system.exists(cookedMeshPath(file_system, eastl::string(kGuid))));

  manager.setAssetCompiler({});
  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  loadMeshMissingFinalFastPathRequestsCook();
  loadTextureMissingFinalFastPathRequestsCook();
  loadMeshStaleMetaFastPathRequestsCook();
  loadMeshFreshFinalPreferred();
  loadMeshLegacyGltfFastPathStillWorks();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "asset_manager_fast_path_test: all passed\n");
  }

  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
