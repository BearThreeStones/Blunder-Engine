#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/inspector_asset_ops.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"

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

void expect_eq_str(const char* label, const eastl::string& actual,
                   const char* expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s (got '%s' want '%s')\n", label,
                 actual.c_str(), expected);
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
      ("blunder_inspector_asset_ops_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Meshes");
  return root;
}

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  expect_true("mesh descriptor path recognized",
              isMeshAssetDescriptorPath(eastl::string("assets/Meshes/Sponza.mesh.yaml")));
  expect_true("texture descriptor path rejected",
              !isMeshAssetDescriptorPath(eastl::string("assets/Textures/foo.texture.yaml")));
  expect_eq_str("display name from path",
                meshAssetDisplayNameFromPath(
                    eastl::string("assets/Meshes/Sponza.mesh.yaml")),
                "Sponza.mesh.yaml");

  const fs::path project = makeTempProject();
  const char* kGuid = "11111111-2222-4333-8444-555555555555";
  const char* kDescriptorPath = "assets/Meshes/sponza.mesh.yaml";
  const char* kIntermediate = "resources/Models/sponza.gltf";

  writeTextFile(project / "Assets" / "Meshes" / "sponza.mesh.yaml",
                std::string("type: Mesh\n") + "guid: " + kGuid + "\n" +
                    "source: " + kIntermediate + "\n" +
                    "import:\n  materials: false\n  animations: false\n"
                    "  scale: 1\n");

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetInspectorIdentity identity{};
  expect_true("resolve mesh asset identity from descriptor yaml",
              resolveMeshAssetInspectorIdentity(eastl::string(kDescriptorPath),
                                                nullptr, &file_system,
                                                identity));
  expect_eq_str("identity display name", identity.display_name, "sponza.mesh.yaml");
  expect_eq_str("identity guid from yaml", identity.guid, kGuid);
  expect_eq_str("identity type", identity.type_label, "Mesh");
  expect_eq_str("identity intermediate path", identity.intermediate_path,
                kIntermediate);

  file_system.shutdown();
  fs::remove_all(project);

  if (g_failures != 0) {
    std::fprintf(stderr, "inspector_asset_ops_test: %d failure(s)\n", g_failures);
    return 1;
  }

  std::fprintf(stdout, "inspector_asset_ops_test: all passed\n");
  std::fflush(stdout);
  return 0;
}
