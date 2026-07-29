#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <cctype>
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
      ("blunder_asset_import_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  fs::create_directories(root / "Assets" / "Textures");
  fs::create_directories(root / "Resources" / "Models");
  fs::create_directories(root / "Resources" / "Textures");
  fs::create_directories(root / "Resources" / "Source");
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

bool looksLikeGltfJson(const std::string& body) {
  return body.find("\"asset\"") != std::string::npos &&
         body.find("\"version\"") != std::string::npos;
}

bool looksLikeGlb(const std::string& body) {
  return body.size() >= 4 && body[0] == 'g' && body[1] == 'l' &&
         body[2] == 'T' && body[3] == 'F';
}

bool looksLikeGltfIntermediate(const std::string& body) {
  return looksLikeGltfJson(body) || looksLikeGlb(body);
}

// Minimal glTF 2.0 triangle (Intermediate-direct import fixture).
constexpr const char* kTriangleGltf = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0 }],
  "meshes": [{
    "primitives": [{
      "attributes": { "POSITION": 0 },
      "indices": 1
    }]
  }],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "max": [1.0, 1.0, 0.0],
      "min": [0.0, 0.0, 0.0]
    },
    {
      "bufferView": 1,
      "componentType": 5123,
      "count": 3,
      "type": "SCALAR"
    }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
  ],
  "buffers": [{
    "byteLength": 42,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"
  }]
})";

// Task 1.1 (ADR 0019): mesh Intermediate = glTF/GLB; COLLADA removed.
// FBX/OBJ are Source Export inputs. Images remain Intermediate-direct.
void meshExtensionRoutingTables() {
  using namespace Blunder;

  expect_true("gltf is mesh Intermediate extension",
              AssetImportService::isMeshIntermediateExtension(".gltf"));
  expect_true("glb is mesh Intermediate extension",
              AssetImportService::isMeshIntermediateExtension(".glb"));
  expect_true("dae is not mesh Intermediate extension",
              !AssetImportService::isMeshIntermediateExtension(".dae"));
  expect_true("gltf is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".gltf"));
  expect_true("glb is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".glb"));
  expect_true("obj is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".obj"));
  expect_true("fbx is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".fbx"));
  expect_true("blend is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".blend"));
  expect_true("blend is not mesh Intermediate extension",
              !AssetImportService::isMeshIntermediateExtension(".blend"));
  expect_true("png is texture Intermediate extension",
              AssetImportService::isTextureIntermediateExtension(".png"));
}

// Task 1.1 (ADR 0019): COLLADA `.dae` is not mesh Intermediate — Import rejects.
void importColladaMeshRejected() {
  using namespace Blunder;
  ensureLogger();

  expect_true("dae is not mesh Intermediate extension",
              !AssetImportService::isMeshIntermediateExtension(".dae"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_dae_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.dae";
  writeTextFile(external, "dae-not-intermediate");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("dae mesh Import rejected", !result.success);
  expect_true("dae reject leaves guid empty", result.guid.empty());
  expect_true("dae reject writes no Assets descriptor",
              !fs::exists(project / "Assets" / "Meshes" / "cube.mesh.yaml"));
  expect_true("dae reject writes no Intermediate under Models",
              !fs::exists(project / "Resources" / "Models" / "cube.dae"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 5.1 / 1.1: Import registers Intermediate (not Source) — copy body under
// Resources (non-Source) + write Assets descriptor with Intermediate `source`.
// Mesh Intermediate-direct input is glTF/GLB (ADR 0019).
void importMeshWritesIntermediateAndDescriptor() {
  using namespace Blunder;
  ensureLogger();

  expect_true("gltf is mesh Intermediate extension",
              AssetImportService::isMeshIntermediateExtension(".gltf"));
  expect_true("png is texture Intermediate extension",
              AssetImportService::isTextureIntermediateExtension(".png"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_ext_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.gltf";
  writeTextFile(external, kTriangleGltf);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("importMesh succeeds", result.success);
  expect_true("importMesh returns guid", !result.guid.empty());
  expect_true("descriptor virtual path under assets/",
              startsWith(result.descriptor_virtual_path, "assets/"));
  expect_true("descriptor ends with .mesh.yaml",
              result.descriptor_virtual_path.find(".mesh.yaml") !=
                  eastl::string::npos);

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  expect_true("Assets descriptor file exists",
              file_system.exists(descriptor_absolute));

  eastl::string yaml;
  expect_true("read descriptor yaml",
              file_system.readText(descriptor_absolute, yaml));

  MeshAssetDescriptor parsed{};
  expect_true("parse mesh descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("descriptor guid matches result", parsed.guid == result.guid);
  expect_true("descriptor source is Intermediate resources/ path",
              startsWith(parsed.source, "resources/"));
  expect_true("descriptor source is not Source archive",
              !containsIgnoreCase(parsed.source, "/source/"));
  expect_true("archived_source empty for glTF Intermediate Import",
              parsed.archived_source.empty());

  eastl::string intermediate_rel = parsed.source;
  if (startsWith(intermediate_rel, "resources/")) {
    intermediate_rel.erase(0, 10);
  }
  const fs::path intermediate_absolute =
      file_system.resolveResource(fs::path(intermediate_rel.c_str()));
  expect_true("Intermediate body exists under Resources",
              file_system.exists(intermediate_absolute));
  expect_true("Intermediate body not under Resources/Source",
              intermediate_absolute.generic_string().find("/Source/") ==
                      std::string::npos &&
                  intermediate_absolute.generic_string().find("\\Source\\") ==
                      std::string::npos);
  expect_true("Intermediate body content copied",
              readTextFile(intermediate_absolute) == kTriangleGltf);
  expect_true("Intermediate body is glTF",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("registry maps guid to descriptor",
              registry.resolveGuid(result.guid) ==
                  result.descriptor_virtual_path);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Minimal Wavefront OBJ triangle (Source Export fixture).
constexpr const char* kTriangleObj = R"(# blunder Source Export fixture
o Triangle
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 0.0 1.0 0.0
f 1 2 3
)";

// Task 1.2 (ADR 0019): Source Export dual-writes Source archive + Intermediate glTF.
void importObjSourceExportDualWritesArchiveAndIntermediate() {
  using namespace Blunder;
  ensureLogger();

  expect_true("obj is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".obj"));
  expect_true("fbx is Source Export extension",
              AssetImportService::isMeshSourceExportExtension(".fbx"));
  expect_true("gltf is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".gltf"));
  expect_true("glb is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".glb"));
  expect_true("blend is not Source Export extension",
              !AssetImportService::isMeshSourceExportExtension(".blend"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_obj_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.obj";
  writeTextFile(external, kTriangleObj);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("OBJ Source Export succeeds", result.success);
  expect_true("OBJ Source Export returns guid", !result.guid.empty());
  expect_true("OBJ descriptor under assets/",
              startsWith(result.descriptor_virtual_path, "assets/"));
  expect_true("OBJ descriptor ends with .mesh.yaml",
              result.descriptor_virtual_path.find(".mesh.yaml") !=
                  eastl::string::npos);

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  expect_true("OBJ Assets descriptor exists",
              file_system.exists(descriptor_absolute));

  eastl::string yaml;
  expect_true("read OBJ descriptor yaml",
              file_system.readText(descriptor_absolute, yaml));

  MeshAssetDescriptor parsed{};
  expect_true("parse OBJ mesh descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("OBJ descriptor guid matches result", parsed.guid == result.guid);
  expect_true("OBJ source is Intermediate resources/ path",
              startsWith(parsed.source, "resources/"));
  expect_true("OBJ source is Intermediate glTF",
              containsIgnoreCase(parsed.source, ".gltf") ||
                  containsIgnoreCase(parsed.source, ".glb"));
  expect_true("OBJ source is not COLLADA .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("OBJ source is not under Source archive",
              !containsIgnoreCase(parsed.source, "/source/"));
  expect_true("OBJ archived_source set", !parsed.archived_source.empty());
  expect_true("OBJ archived_source under Source",
              containsIgnoreCase(parsed.archived_source, "source/"));
  expect_true("OBJ archived_source keeps .obj",
              containsIgnoreCase(parsed.archived_source, ".obj"));

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  const fs::path intermediate_absolute = resolveResourcesVirtual(parsed.source);
  expect_true("Intermediate glTF exists under Resources",
              file_system.exists(intermediate_absolute));
  expect_true("Intermediate glTF not under Resources/Source",
              intermediate_absolute.generic_string().find("/Source/") ==
                      std::string::npos &&
                  intermediate_absolute.generic_string().find("\\Source\\") ==
                      std::string::npos);
  expect_true("Intermediate lives under Models",
              containsIgnoreCase(eastl::string(intermediate_absolute.generic_string().c_str()),
                                 "/models/") ||
                  containsIgnoreCase(
                      eastl::string(intermediate_absolute.generic_string().c_str()),
                      "\\models\\"));
  const std::string intermediate_body = readTextFile(intermediate_absolute);
  expect_true("Intermediate body looks like glTF",
              looksLikeGltfIntermediate(intermediate_body));

  eastl::string archived_rel = parsed.archived_source;
  if (startsWith(archived_rel, "resources/")) {
    archived_rel.erase(0, 10);
  }
  const fs::path archived_absolute =
      file_system.resolveResource(fs::path(archived_rel.c_str()));
  expect_true("archived Source file exists under Resources/Source",
              file_system.exists(archived_absolute));
  expect_true("archived Source path contains Source",
              archived_absolute.generic_string().find("/Source/") !=
                      std::string::npos ||
                  archived_absolute.generic_string().find("\\Source\\") !=
                      std::string::npos);
  expect_true("archived Source content matches OBJ",
              readTextFile(archived_absolute) == kTriangleObj);

  expect_true("registry maps OBJ guid to descriptor",
              registry.resolveGuid(result.guid) ==
                  result.descriptor_virtual_path);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 1.1 (ADR 0019): glTF Import is Intermediate-direct, not Source Export.
void importGltfIntermediateDirectNotSourceExport() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_gltf_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.gltf";
  writeTextFile(external, kTriangleGltf);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);

  expect_true("glTF Intermediate Import succeeds", result.success);
  expect_true("glTF Intermediate Import returns guid", !result.guid.empty());

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("read glTF descriptor yaml",
              file_system.readText(descriptor_absolute, yaml));

  MeshAssetDescriptor parsed{};
  expect_true("parse glTF mesh descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("glTF source is Intermediate .gltf",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("glTF source is not COLLADA .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("glTF archived_source empty (no Source Export)",
              parsed.archived_source.empty());

  auto resolveResourcesVirtual = [&](const eastl::string& virtual_path) {
    eastl::string relative = virtual_path;
    if (startsWith(relative, "resources/")) {
      relative.erase(0, 10);
    }
    return file_system.resolveResource(fs::path(relative.c_str()));
  };

  const fs::path intermediate_absolute = resolveResourcesVirtual(parsed.source);
  expect_true("glTF Intermediate file exists",
              file_system.exists(intermediate_absolute));
  expect_true("glTF Intermediate body preserved",
              readTextFile(intermediate_absolute) == kTriangleGltf);
  expect_true("glTF Import did not archive under Source",
              !fs::exists(project / "Resources" / "Source" / "Models"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Minimal Wavefront OBJ quad (distinct from triangle for Reimport refresh).
constexpr const char* kQuadObj = R"(# blunder Reimport fixture
o Quad
v 0.0 0.0 0.0
v 1.0 0.0 0.0
v 1.0 1.0 0.0
v 0.0 1.0 0.0
f 1 2 3
f 1 3 4
)";

// Task 5.3: Reimport from archived Source preserves GUID and refreshes Intermediate.
void reimportObjPreservesGuidAndRefreshesIntermediate() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_obj_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.obj";
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

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("Reimport fixture: OBJ import succeeds", imported.success);
  expect_true("Reimport fixture: GUID allocated", !imported.guid.empty());
  const eastl::string original_guid = imported.guid;

  eastl::string desc_rel = imported.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("Reimport fixture: read descriptor",
              file_system.readText(descriptor_absolute, yaml));
  MeshAssetDescriptor parsed{};
  expect_true("Reimport fixture: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));

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
  expect_true("Reimport fixture: Intermediate exists",
              file_system.exists(intermediate_absolute));
  expect_true("Reimport fixture: archived Source exists",
              file_system.exists(archived_absolute));

  // Stamp Intermediate so we can detect a real Assimp re-export overwrite.
  writeTextFile(intermediate_absolute, "STALE_INTERMEDIATE_MARKER");
  expect_true("Reimport fixture: Intermediate stamped stale",
              readTextFile(intermediate_absolute) ==
                  "STALE_INTERMEDIATE_MARKER");

  // Plant a cooked Final so Reimport must invalidate it.
  const fs::path mesh_cooked = cookedMeshPath(file_system, original_guid);
  const fs::path mesh_meta = cookedMeshMetaPath(file_system, original_guid);
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");
  expect_true("Reimport fixture: planted cooked Final",
              file_system.exists(mesh_cooked));

  // Modify archived Source (simulates artist edit / watch trigger input).
  writeTextFile(archived_absolute, kQuadObj);

  expect_true("requestReimport succeeds",
              import_service.requestReimport(original_guid));

  expect_true("Reimport preserves GUID in registry",
              registry.resolveGuid(original_guid) ==
                  imported.descriptor_virtual_path);

  eastl::string yaml_after;
  expect_true("Reimport: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed_after{};
  expect_true("Reimport: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed_after));
  expect_true("Reimport preserves descriptor GUID",
              parsed_after.guid == original_guid);
  expect_true("Reimport keeps Intermediate source path",
              parsed_after.source == parsed.source);
  expect_true("Reimport keeps archived_source path",
              parsed_after.archived_source == parsed.archived_source);

  const std::string intermediate_after =
      readTextFile(intermediate_absolute);
  expect_true("Reimport refreshes Intermediate (not stale marker)",
              intermediate_after != "STALE_INTERMEDIATE_MARKER");
  expect_true("Reimport Intermediate still looks like glTF",
              looksLikeGltfIntermediate(intermediate_after));

  expect_true("Reimport invalidates cooked Final",
              !file_system.exists(mesh_cooked));
  expect_true("Reimport invalidates cooked meta",
              !file_system.exists(mesh_meta));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 5.3: Intermediate-only Reimport preserves GUID and invalidates Finals.
void reimportIntermediateOnlyPreservesGuidAndInvalidatesFinal() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_gltf_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.gltf";
  writeTextFile(external, kTriangleGltf);

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

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("glTF Intermediate Reimport fixture import succeeds", imported.success);
  const eastl::string original_guid = imported.guid;

  const fs::path mesh_cooked = cookedMeshPath(file_system, original_guid);
  const fs::path mesh_meta = cookedMeshMetaPath(file_system, original_guid);
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  expect_true("Intermediate-only requestReimport succeeds",
              import_service.requestReimport(original_guid));
  expect_true("Intermediate-only Reimport preserves GUID",
              registry.resolveGuid(original_guid) ==
                  imported.descriptor_virtual_path);
  expect_true("Intermediate-only Reimport invalidates Final",
              !file_system.exists(mesh_cooked));
  expect_true("Intermediate-only Reimport invalidates meta",
              !file_system.exists(mesh_meta));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

void importUnsupportedSourceExportRejected() {
  using namespace Blunder;
  ensureLogger();

  expect_true("blend is not Source Export whitelist",
              !AssetImportService::isMeshSourceExportExtension(".blend"));
  expect_true("blend is not Intermediate mesh extension",
              !AssetImportService::isMeshIntermediateExtension(".blend"));

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_blend_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "cube.blend";
  writeTextFile(external, "not-a-real-blend");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult result =
      import_service.importMesh(external, "assets/Meshes", settings);
  // Task 5.4: .blend is not Assimp Source Export whitelist — clear failure,
  // not silent success or copy-to-Source-only.
  expect_true("blend Source Export rejected", !result.success);
  expect_true("blend reject leaves guid empty", result.guid.empty());
  expect_true("blend reject leaves descriptor path empty",
              result.descriptor_virtual_path.empty());
  expect_true("blend reject writes no Assets descriptor",
              !fs::exists(project / "Assets" / "Meshes" / "cube.mesh.yaml"));
  expect_true("blend reject writes no Source archive",
              !fs::exists(project / "Resources" / "Source" / "Models" /
                          "cube"));
  expect_true("blend reject writes no Intermediate under Models",
              !fs::exists(project / "Resources" / "Models" / "cube.dae") &&
                  !fs::exists(project / "Resources" / "Models" / "cube.gltf") &&
                  !fs::exists(project / "Resources" / "Models" / "cube.glb"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

void importTextureWritesIntermediateAndDescriptor() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_import_tex_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "albedo.png";
  writeTextFile(external, "png-intermediate-body");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  TextureImportSettings settings{};
  const ImportResult result =
      import_service.importTexture(external, "assets/Textures", settings);

  expect_true("importTexture succeeds", result.success);
  expect_true("texture descriptor under assets/",
              startsWith(result.descriptor_virtual_path, "assets/"));

  eastl::string desc_rel = result.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("read texture descriptor",
              file_system.readText(descriptor_absolute, yaml));

  TextureAssetDescriptor parsed{};
  expect_true("parse texture descriptor",
              AssetYaml::parseTextureDescriptor(yaml, parsed));
  expect_true("texture source is Intermediate resources/ path",
              startsWith(parsed.source, "resources/"));
  expect_true("texture source is not Source archive",
              !containsIgnoreCase(parsed.source, "/source/"));
  expect_true("texture archived_source empty",
              parsed.archived_source.empty());

  eastl::string intermediate_rel = parsed.source;
  if (startsWith(intermediate_rel, "resources/")) {
    intermediate_rel.erase(0, 10);
  }
  const fs::path intermediate_absolute =
      file_system.resolveResource(fs::path(intermediate_rel.c_str()));
  expect_true("texture Intermediate body exists under Resources",
              file_system.exists(intermediate_absolute));
  expect_true("texture Intermediate body content copied",
              readTextFile(intermediate_absolute) == "png-intermediate-body");

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

// Task 1.2 (ADR 0019): registry scan must not convert glTF Intermediate → COLLADA.
void registryScanDoesNotUpgradeGltfToDae() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  fs::create_directories(project / "Resources" / "Models" / "legacy");
  fs::create_directories(project / "Resources" / "Source");

  const fs::path legacy_gltf =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  writeTextFile(legacy_gltf, kTriangleGltf);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.gltf\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

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

  const uint32_t upgraded = import_service.scanAndUpgradeLegacyIntermediates();
  expect_true("registry scan reports zero glTF→dae upgrades", upgraded == 0);

  eastl::string yaml_after;
  expect_true("no-upgrade: read descriptor after scan",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("no-upgrade: parse descriptor after scan",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("no-upgrade preserves descriptor GUID", parsed.guid == kGuid);
  expect_true("no-upgrade keeps glTF source",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("no-upgrade source does not point at .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("no-upgrade does not invent archived_source",
              parsed.archived_source.empty());

  const fs::path sibling_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  expect_true("no-upgrade does not create sibling .dae",
              !file_system.exists(sibling_dae));
  expect_true("no-upgrade preserves legacy glTF Intermediate",
              file_system.exists(legacy_gltf));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.2: upgradeLegacyMeshIntermediates is a no-op (glTF stays glTF).
void upgradeLegacyMeshIntermediatesIsNoOp() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_gltf =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  writeTextFile(legacy_gltf, kTriangleGltf);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  const std::string descriptor_before =
      std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
      "source: resources/Models/legacy/hero.gltf\n" +
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1\n";
  writeTextFile(descriptor_absolute, descriptor_before);

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  const uint32_t upgraded = import_service.upgradeLegacyMeshIntermediates();
  expect_true("upgradeLegacyMeshIntermediates returns zero", upgraded == 0);

  eastl::string yaml_after;
  expect_true("no-op upgrade: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  expect_true("no-op upgrade: descriptor body unchanged",
              std::string(yaml_after.c_str()) == descriptor_before);

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Legacy migration: minimal COLLADA Intermediate (Assimp-readable).
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

// Task 1.4 (ADR 0019): legacy `.dae` Intermediate migrates GUID-preserving to glTF.
void upgradeLegacyDaeIntermediateToGltfPreservesGuid() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "dddddddd-eeee-4fff-8aaa-bbbbbbbbbbbb";

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.dae\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

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

  const fs::path mesh_cooked = cookedMeshPath(file_system, kGuid);
  writeBinaryFile(mesh_cooked, "MESH", 4);

  const uint32_t migrated = import_service.upgradeLegacyMeshIntermediates();
  expect_true("dae migration reports one Asset", migrated == 1);

  eastl::string yaml_after;
  expect_true("dae migration: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("dae migration: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("dae migration preserves descriptor GUID", parsed.guid == kGuid);
  expect_true("dae migration rewrites source to glTF",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("dae migration source does not point at .dae",
              !containsIgnoreCase(parsed.source, ".dae"));
  expect_true("dae migration archives former .dae under Source",
              !parsed.archived_source.empty());
  expect_true("dae migration archived_source under Source",
              containsIgnoreCase(parsed.archived_source, "source/"));
  expect_true("dae migration archived_source keeps .dae",
              containsIgnoreCase(parsed.archived_source, ".dae"));

  const fs::path gltf_path =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  expect_true("dae migration creates sibling glTF Intermediate",
              file_system.exists(gltf_path));
  expect_true("dae migration glTF body looks valid",
              looksLikeGltfIntermediate(readTextFile(gltf_path)));
  expect_true("dae migration removes legacy .dae Intermediate",
              !file_system.exists(legacy_dae));
  expect_true("dae migration invalidates cooked Final",
              !file_system.exists(mesh_cooked));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.4: registry scan path migrates legacy `.dae` Intermediate.
void registryScanUpgradesDaeToGltf() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "eeeeeeee-ffff-4aaa-8bbb-cccccccccccc";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.dae\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  const uint32_t migrated = import_service.scanAndUpgradeLegacyIntermediates();
  expect_true("scan migrates legacy dae", migrated == 1);

  eastl::string yaml_after;
  expect_true("scan dae migration: read descriptor",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("scan dae migration: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("scan dae migration preserves GUID", parsed.guid == kGuid);
  expect_true("scan dae migration rewrites source to glTF",
              containsIgnoreCase(parsed.source, ".gltf"));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.4: fail-soft migration leaves `.dae` source when convert fails.
void upgradeLegacyDaeFailSoftLeavesDaeSource() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "ffffff00-1111-4222-8333-444444444444";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path partial_gltf =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  writeTextFile(partial_gltf, "PARTIAL_GLTF_NOT_VALID");

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  const std::string descriptor_before =
      std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
      "source: resources/Models/legacy/hero.dae\n" +
      "import:\n"
      "  materials: true\n"
      "  animations: true\n"
      "  scale: 1\n";
  writeTextFile(descriptor_absolute, descriptor_before);

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

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  AssetImportService::setForceUpgradeConvertFailureForTest(true);
  const uint32_t migrated = import_service.upgradeLegacyMeshIntermediates();
  AssetImportService::setForceUpgradeConvertFailureForTest(false);

  expect_true("fail-soft dae migration reports zero", migrated == 0);

  eastl::string yaml_after;
  expect_true("fail-soft dae: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  expect_true("fail-soft dae: descriptor body unchanged",
              std::string(yaml_after.c_str()) == descriptor_before);

  MeshAssetDescriptor parsed{};
  expect_true("fail-soft dae: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("fail-soft dae: source still .dae",
              containsIgnoreCase(parsed.source, ".dae"));
  expect_true("fail-soft dae: archived_source not invented",
              parsed.archived_source.empty());
  expect_true("fail-soft dae: cleans partial sibling glTF",
              !file_system.exists(partial_gltf));
  expect_true("fail-soft dae: legacy Intermediate preserved",
              file_system.exists(legacy_dae));

  const eastl::shared_ptr<MeshAsset> mesh =
      manager.loadMesh(eastl::string("assets/Meshes/hero.mesh.yaml"));
  expect_true("fail-soft dae: loadMesh legacy Fast Path returns mesh",
              mesh != nullptr);
  expect_true("fail-soft dae: loadMesh legacy Fast Path has vertices",
              mesh && mesh->getVertexCount() == 3);

  import_service.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.4: when archived OBJ exists, migration Reimports glTF from Source.
void upgradeLegacyDaeWithArchivedObjReimportsGltf() {
  using namespace Blunder;
  ensureLogger();

  constexpr const char* kGuid = "aaaa1111-bbbb-4222-8ccc-dddddddddddd";

  const fs::path project = makeTempProject();
  fs::create_directories(project / "Resources" / "Models" / "legacy");
  fs::create_directories(project / "Resources" / "Source" / "Models" / "hero");

  const fs::path legacy_dae =
      project / "Resources" / "Models" / "legacy" / "hero.dae";
  writeTextFile(legacy_dae, kMinimalTriangleDae);

  const fs::path archived_obj =
      project / "Resources" / "Source" / "Models" / "hero" / "triangle.obj";
  writeTextFile(archived_obj, kTriangleObj);

  const fs::path descriptor_absolute =
      project / "Assets" / "Meshes" / "hero.mesh.yaml";
  writeTextFile(descriptor_absolute,
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: resources/Models/legacy/hero.dae\n" +
                    "archived_source: resources/Source/Models/hero/triangle.obj\n" +
                    "import:\n"
                    "  materials: true\n"
                    "  animations: true\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init{};
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_service.initialize(import_init);

  const uint32_t migrated = import_service.upgradeLegacyMeshIntermediates();
  expect_true("archived OBJ dae migration reports one", migrated == 1);

  eastl::string yaml_after;
  expect_true("archived OBJ migration: read descriptor",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed{};
  expect_true("archived OBJ migration: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed));
  expect_true("archived OBJ migration preserves GUID", parsed.guid == kGuid);
  expect_true("archived OBJ migration rewrites source to glTF",
              containsIgnoreCase(parsed.source, ".gltf"));
  expect_true("archived OBJ migration keeps archived_source",
              parsed.archived_source ==
                  "resources/Source/Models/hero/triangle.obj");

  const fs::path gltf_path =
      project / "Resources" / "Models" / "legacy" / "hero.gltf";
  expect_true("archived OBJ migration creates glTF Intermediate",
              file_system.exists(gltf_path));
  expect_true("archived OBJ migration glTF from OBJ reexport",
              looksLikeGltfIntermediate(readTextFile(gltf_path)));

  import_service.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
}

// Task 1.2: Reimport from archived OBJ regenerates Intermediate glTF.
void reimportFromArchivedObjRegeneratesGltfIntermediate() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  fs::create_directories(project / ".blunder" / "cooked");
  const fs::path external =
      fs::temp_directory_path() /
      ("blunder_reimport_gltf_obj_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count()))) /
      "triangle.obj";
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

  AssetCompilerService compiler;
  compiler.initialize(&file_system, &manager, &registry);

  AssetImportService import_service;
  AssetImportServiceInit import_init{};
  import_init.file_system = &file_system;
  import_init.asset_registry = &registry;
  import_init.asset_compiler = &compiler;
  import_service.initialize(import_init);

  MeshImportSettings settings{};
  const ImportResult imported =
      import_service.importMesh(external, "assets/Meshes", settings);
  expect_true("gltf reimport fixture: OBJ import succeeds", imported.success);
  const eastl::string original_guid = imported.guid;

  eastl::string desc_rel = imported.descriptor_virtual_path;
  if (startsWith(desc_rel, "assets/")) {
    desc_rel.erase(0, 7);
  }
  const fs::path descriptor_absolute =
      file_system.resolveAsset(fs::path(desc_rel.c_str()));
  eastl::string yaml;
  expect_true("gltf reimport fixture: read descriptor",
              file_system.readText(descriptor_absolute, yaml));
  MeshAssetDescriptor parsed{};
  expect_true("gltf reimport fixture: parse descriptor",
              AssetYaml::parseMeshDescriptor(yaml, parsed));
  expect_true("gltf reimport fixture: source is glTF",
              containsIgnoreCase(parsed.source, ".gltf") ||
                  containsIgnoreCase(parsed.source, ".glb"));

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
  expect_true("gltf reimport fixture: Intermediate exists",
              file_system.exists(intermediate_absolute));
  expect_true("gltf reimport fixture: archived Source exists",
              file_system.exists(archived_absolute));

  writeTextFile(intermediate_absolute, "STALE_GLTF_MARKER");
  expect_true("gltf reimport fixture: Intermediate stamped stale",
              readTextFile(intermediate_absolute) == "STALE_GLTF_MARKER");

  const fs::path mesh_cooked = cookedMeshPath(file_system, original_guid);
  const fs::path mesh_meta = cookedMeshMetaPath(file_system, original_guid);
  writeBinaryFile(mesh_cooked, "MESH", 4);
  writeTextFile(mesh_meta, "source_mtime: 1\ndescriptor_mtime: 2\n");

  writeTextFile(archived_absolute, kQuadObj);

  expect_true("gltf reimport: requestReimport succeeds",
              import_service.requestReimport(original_guid));

  expect_true("gltf reimport preserves GUID",
              registry.resolveGuid(original_guid) ==
                  imported.descriptor_virtual_path);

  eastl::string yaml_after;
  expect_true("gltf reimport: read descriptor after",
              file_system.readText(descriptor_absolute, yaml_after));
  MeshAssetDescriptor parsed_after{};
  expect_true("gltf reimport: parse descriptor after",
              AssetYaml::parseMeshDescriptor(yaml_after, parsed_after));
  expect_true("gltf reimport keeps Intermediate source path",
              parsed_after.source == parsed.source);

  const std::string intermediate_after = readTextFile(intermediate_absolute);
  expect_true("gltf reimport refreshes Intermediate (not stale marker)",
              intermediate_after != "STALE_GLTF_MARKER");
  expect_true("gltf reimport regenerated body is glTF",
              looksLikeGltfIntermediate(intermediate_after));
  expect_true("gltf reimport invalidates cooked Final",
              !file_system.exists(mesh_cooked));

  import_service.shutdown();
  compiler.shutdown();
  manager.shutdown();
  registry.shutdown();
  file_system.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  fs::remove_all(project);
  fs::remove_all(external.parent_path());
}

}  // namespace

int main() {
  meshExtensionRoutingTables();
  importColladaMeshRejected();
  importMeshWritesIntermediateAndDescriptor();
  importTextureWritesIntermediateAndDescriptor();
  importObjSourceExportDualWritesArchiveAndIntermediate();
  importGltfIntermediateDirectNotSourceExport();
  importUnsupportedSourceExportRejected();
  reimportObjPreservesGuidAndRefreshesIntermediate();
  reimportIntermediateOnlyPreservesGuidAndInvalidatesFinal();
  registryScanDoesNotUpgradeGltfToDae();
  upgradeLegacyMeshIntermediatesIsNoOp();
  upgradeLegacyDaeIntermediateToGltfPreservesGuid();
  registryScanUpgradesDaeToGltf();
  upgradeLegacyDaeFailSoftLeavesDaeSource();
  upgradeLegacyDaeWithArchivedObjReimportsGltf();
  reimportFromArchivedObjRegeneratesGltfIntermediate();
  if (g_failures != 0) {
    std::fprintf(stderr, "asset_import_test: %d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "asset_import_test: all passed\n");
  return 0;
}
