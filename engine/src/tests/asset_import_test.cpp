#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_import/asset_import_service.h"
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

// Task 5.1: Import registers Intermediate (not Source) — copy body under
// Resources (non-Source) + write Assets descriptor with Intermediate `source`.
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
  writeTextFile(external, "gltf-intermediate-body");

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
  expect_true("archived_source empty for glTF Import",
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
              readTextFile(intermediate_absolute) == "gltf-intermediate-body");
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

}  // namespace

int main() {
  importMeshWritesIntermediateAndDescriptor();
  importTextureWritesIntermediateAndDescriptor();
  if (g_failures != 0) {
    std::fprintf(stderr, "asset_import_test: %d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "asset_import_test: all passed\n");
  return 0;
}
