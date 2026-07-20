#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/guid.h"
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
      ("blunder_asset_registry_test_" +
       std::to_string(
           static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Scenes");
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

void rebuildFromScanRegistersSceneWithGuid() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kGuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  const std::string scene_json =
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
      kGuid + "\",\n" + "  \"entities\": [],\n" + "  \"childScenes\": []\n" +
      "}\n";
  writeTextFile(project / "Assets" / "Scenes" / "guided.scene.asset",
                scene_json);

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  const eastl::string resolved = registry.resolveGuid(eastl::string(kGuid));
  expect_true("scan registers scene with guid",
              resolved == "assets/Scenes/guided.scene.asset");
  expect_true(
      "scan findGuidForPath for guided scene",
      registry.findGuidForPath("assets/Scenes/guided.scene.asset") == kGuid);

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void ensureUpgradesLegacySceneWithoutGuid() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const std::string legacy_json =
      "{\n"
      "  \"type\": \"Scene\",\n"
      "  \"entities\": [],\n"
      "  \"childScenes\": []\n"
      "}\n";
  const fs::path scene_path =
      project / "Assets" / "Scenes" / "legacy.scene.asset";
  writeTextFile(scene_path, legacy_json);

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);
  registry.rebuildFromScan();

  expect_true(
      "legacy scene absent from registry before ensure",
      registry.findGuidForPath("assets/Scenes/legacy.scene.asset").empty());

  expect_true("ensureSceneAssetRegistered succeeds",
              registry.ensureSceneAssetRegistered(
                  "assets/Scenes/legacy.scene.asset"));

  const eastl::string guid =
      registry.findGuidForPath("assets/Scenes/legacy.scene.asset");
  expect_true("ensure allocated valid guid", isValidGuidFormat(guid));
  expect_true("registry resolves upgraded scene guid",
              registry.resolveGuid(guid) == "assets/Scenes/legacy.scene.asset");

  const std::string on_disk = readTextFile(scene_path);
  expect_true("upgraded scene file contains guid key",
              on_disk.find("\"guid\"") != std::string::npos);
  expect_true("upgraded scene file contains allocated guid",
              on_disk.find(guid.c_str()) != std::string::npos);

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void ensureRegistersExistingGuidWhenMissingFromRegistry() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  // Write after initialize so rebuildFromScan did not see this file yet.
  const char* kGuid = "11111111-2222-3333-4444-555555555555";
  const std::string scene_json =
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
      kGuid + "\",\n" + "  \"entities\": [],\n" + "  \"childScenes\": []\n" +
      "}\n";
  writeTextFile(project / "Assets" / "Scenes" / "already.scene.asset",
                scene_json);

  expect_true(
      "scene with guid missing from registry before ensure",
      registry.findGuidForPath("assets/Scenes/already.scene.asset").empty());
  expect_true(
      "ensure registers scene that already has guid",
      registry.ensureSceneAssetRegistered("assets/Scenes/already.scene.asset"));
  expect_true("existing guid preserved",
              registry.findGuidForPath("assets/Scenes/already.scene.asset") ==
                  kGuid);
  expect_true("resolve existing guid path",
              registry.resolveGuid(eastl::string(kGuid)) ==
                  "assets/Scenes/already.scene.asset");

  const std::string on_disk =
      readTextFile(project / "Assets" / "Scenes" / "already.scene.asset");
  expect_true("existing guid not rewritten away",
              on_disk.find(kGuid) != std::string::npos);

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

void ensureRewritesInvalidGuidOnDisk() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const char* kBadGuid = "not-a-valid-guid";
  const std::string bad_json =
      std::string("{\n") + "  \"type\": \"Scene\",\n" + "  \"guid\": \"" +
      kBadGuid + "\",\n" + "  \"entities\": [],\n" + "  \"childScenes\": []\n" +
      "}\n";
  const fs::path scene_path =
      project / "Assets" / "Scenes" / "bad_guid.scene.asset";
  writeTextFile(scene_path, bad_json);

  FileSystem file_system;
  FileSystemInitInfo init;
  init.project_root = project;
  file_system.initialize(init);

  AssetRegistry registry;
  registry.initialize(&file_system);

  expect_true(
      "ensure recovers from invalid guid",
      registry.ensureSceneAssetRegistered("assets/Scenes/bad_guid.scene.asset"));

  const eastl::string guid =
      registry.findGuidForPath("assets/Scenes/bad_guid.scene.asset");
  expect_true("recovered guid is valid", isValidGuidFormat(guid));
  expect_true("registry resolves recovered guid",
              registry.resolveGuid(guid) ==
                  "assets/Scenes/bad_guid.scene.asset");

  const std::string on_disk = readTextFile(scene_path);
  expect_true("disk no longer contains invalid guid",
              on_disk.find(kBadGuid) == std::string::npos);
  expect_true("disk contains recovered guid matching registry",
              on_disk.find(guid.c_str()) != std::string::npos);

  registry.shutdown();
  file_system.shutdown();
  fs::remove_all(project);
}

}  // namespace

int main() {
  rebuildFromScanRegistersSceneWithGuid();
  ensureUpgradesLegacySceneWithoutGuid();
  ensureRegistersExistingGuidWhenMissingFromRegistry();
  ensureRewritesInvalidGuidOnDisk();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stdout, "asset_registry_test: all passed\n");
  }

  // Destroy async logger before process exit so spdlog's thread pool is still
  // alive (avoids ACCESS_VIOLATION during static teardown).
  Blunder::g_runtime_global_context.m_logger_system.reset();
  return exit_code;
}
