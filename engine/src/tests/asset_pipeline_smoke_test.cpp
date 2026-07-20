#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include "EASTL/shared_ptr.h"

#include <chrono>
#include <cstdio>
#include <cstring>
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
      ("blunder_asset_pipeline_smoke_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Scenes");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Source");
  fs::create_directories(root / ".blunder" / "cooked");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

std::string readTextFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

bool startsWith(const eastl::string& value, const char* prefix) {
  const size_t n = std::strlen(prefix);
  return value.size() >= n && value.compare(0, n, prefix) == 0;
}

bool containsIgnoreCase(const eastl::string& value, const char* needle) {
  std::string lower(value.c_str());
  for (char& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return lower.find(needle) != std::string::npos;
}

constexpr const char* kTriangleObj = R"(# blunder pipeline smoke triangle
o Triangle
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 0.0 1.0 0.0
f 1 2 3
)";

constexpr const char* kQuadObj = R"(# blunder pipeline smoke quad
o Quad
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 1.0 1.0 0.0
v 0.0 1.0 0.0
f 1 2 3
f 1 3 4
)";

/// OpenSpec 6.2 automated smoke (non-UI):
/// Import OBJ dual-write → loadMesh Fast Path → cookAsset Final →
/// Reimport GUID stable → scene mesh GUID serialize.
void smokeObjImportFastPathCookReimportSceneGuid() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_pipeline_smoke_ext_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "smoke_tri.obj";
  writeTextFile(external, kTriangleObj);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  auto compiler = eastl::make_shared<AssetCompilerService>();
  compiler->initialize(&file_system, &manager, &registry);
  manager.setAssetCompiler(compiler);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = compiler.get();
  import_service.initialize(import_init);

  // ---- 1) OBJ Import dual-write ----
  MeshImportSettings settings{};
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("smoke: OBJ Import succeeds", imported.success);
  expect_true("smoke: OBJ Import GUID allocated", !imported.guid.empty());
  expect_true("smoke: OBJ descriptor under assets/",
              startsWith(imported.descriptor_virtual_path, "assets/"));
  const eastl::string original_guid = imported.guid;

  eastl::string desc_rel = imported.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("smoke: read descriptor",
              file_system.readText(descriptor_absolute, yaml));
  MeshAssetDescriptor parsed{};
  expect_true("smoke: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("smoke: dual-write Intermediate source",
              startsWith(parsed.source, "resources/") &&
                  !containsIgnoreCase(parsed.source, "/source/"));
  expect_true("smoke: Intermediate source is COLLADA .dae",
              containsIgnoreCase(parsed.source, ".dae"));
  expect_true("smoke: Intermediate source is not glTF",
              !containsIgnoreCase(parsed.source, ".gltf") &&
                  !containsIgnoreCase(parsed.source, ".glb"));
  expect_true("smoke: dual-write archived_source under Source",
              !parsed.archived_source.empty() &&
                  containsIgnoreCase(parsed.archived_source, "source/"));

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };
  const fs::path intermediate_absolute =
      resolveResourcesVirtual(parsed.source);
  const fs::path archived_absolute =
      resolveResourcesVirtual(parsed.archived_source);
  expect_true("smoke: Intermediate COLLADA exists",
              file_system.exists(intermediate_absolute));
  expect_true("smoke: archived Source exists",
              file_system.exists(archived_absolute));

  expect_true(
      "smoke: precondition Final missing before Fast Path",
      !file_system.exists(cookedMeshPath(file_system, original_guid)));

  // ---- 2) Fast Path load (Intermediate; may request background cook) ----
  const eastl::shared_ptr<MeshAsset> fast_mesh =
      manager.loadMesh(imported.descriptor_virtual_path);
  expect_true("smoke: Fast Path loadMesh returns mesh", fast_mesh != nullptr);
  expect_true("smoke: Fast Path mesh has geometry",
              fast_mesh && fast_mesh->getVertexCount() >= 3);

  // ---- 3) Explicit cookAsset Final ----
  expect_true("smoke: cookAsset writes Final",
              compiler->cookAsset(original_guid, /*force=*/true));
  expect_true(
      "smoke: cooked Final exists",
      file_system.exists(cookedMeshPath(file_system, original_guid)));
  expect_true(
      "smoke: cooked meta exists",
      file_system.exists(cookedMeshMetaPath(file_system, original_guid)));

  // Prefer Final on subsequent load after cook.
  manager.clearCache();
  const eastl::shared_ptr<MeshAsset> cooked_mesh =
      manager.loadMeshByGuid(original_guid, registry);
  expect_true("smoke: loadMeshByGuid after cook succeeds",
              cooked_mesh != nullptr);
  expect_true("smoke: cooked mesh has geometry",
              cooked_mesh && cooked_mesh->getVertexCount() >= 3);

  // ---- 4) Reimport preserves GUID ----
  writeTextFile(archived_absolute, kQuadObj);
  expect_true("smoke: requestReimport succeeds",
              import_service.requestReimport(original_guid));
  expect_true("smoke: Reimport preserves registry GUID mapping",
              registry.resolveGuid(original_guid) ==
                  imported.descriptor_virtual_path);

  eastl::string yaml_after;
  expect_true("smoke: read descriptor after Reimport",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed_after{};
  expect_true("smoke: parse descriptor after Reimport",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed_after));
  expect_true("smoke: Reimport preserves descriptor GUID",
              parsed_after.guid == original_guid);
  expect_true("smoke: Reimport invalidates Final",
              !file_system.exists(cookedMeshPath(file_system, original_guid)));

  // ---- 5) Scene mesh GUID serialize ----
  Scene scene;
  scene.setGuid("bbbbbbbb-cccc-4ddd-8eee-ffffffffffff");
  SceneEntityDefinition entity;
  entity.name = "SmokeMesh";
  entity.mesh_virtual_path = original_guid;
  scene.getEntities().push_back(eastl::move(entity));

  eastl::string json;
  expect_true("smoke: serialize scene with mesh GUID",
              SceneSerializer::serialize(scene, json, &registry));
  const eastl::string mesh_field =
      eastl::string("\"mesh\": \"") + original_guid + "\"";
  expect_true("smoke: scene JSON mesh field is GUID",
              json.find(mesh_field) != eastl::string::npos);
  expect_true("smoke: scene JSON mesh is not a path",
              json.find("assets/") == eastl::string::npos &&
                  json.find(".mesh.yaml") == eastl::string::npos);

  Scene loaded;
  expect_true("smoke: deserialize scene with mesh GUID",
              SceneSerializer::deserialize(json, loaded, &registry));
  expect_true("smoke: loaded entity mesh remains GUID",
              loaded.getEntities().size() == 1 &&
                  loaded.getEntities()[0].mesh_virtual_path == original_guid);

  import_service.shutdown();
  manager.setAssetCompiler({});
  compiler->shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

}  // namespace

int main() {
  smokeObjImportFastPathCookReimportSceneGuid();
  if (g_failures != 0) {
    std::fprintf(stderr, "asset_pipeline_smoke_test: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::fprintf(stdout, "asset_pipeline_smoke_test: all passed\n");
  return 0;
}
