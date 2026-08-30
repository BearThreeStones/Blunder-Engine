#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
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

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

fs::path makeTempProject() {
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_play_reload_scene_test_" +
       std::to_string(static_cast<unsigned long long>(
           std::chrono::steady_clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "Scenes");
  return root;
}

constexpr char kSceneHero[] = R"({
  "type": "Scene",
  "guid": "11111111-2222-4333-8444-555555555555",
  "entities": [
    {
      "name": "Hero",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    }
  ]
})";

constexpr char kSceneHeroAndSidekick[] = R"({
  "type": "Scene",
  "guid": "11111111-2222-4333-8444-555555555555",
  "entities": [
    {
      "name": "Hero",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    },
    {
      "name": "Sidekick",
      "position": [1, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    }
  ]
})";

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  const fs::path project = makeTempProject();
  const eastl::string virtual_path("assets/Scenes/play_entry.scene.asset");
  const fs::path disk = project / "Assets" / "Scenes" / "play_entry.scene.asset";
  writeTextFile(disk, kSceneHero);

  FileSystem file_system;
  FileSystemInitInfo fs_init;
  fs_init.project_root = project;
  file_system.initialize(fs_init);

  AssetManager manager;
  AssetManagerInitInfo am_init;
  am_init.file_system = &file_system;
  manager.initialize(am_init);

  SceneSystem scenes;
  scenes.initialize(SceneSystemInitInfo{&manager});

  expect_true("reload with no active fails", !scenes.reloadActiveFromDisk());

  const eastl::shared_ptr<SceneInstance> first = scenes.loadScene(virtual_path);
  expect_true("loadScene ok", first != nullptr);
  if (!first) {
    std::_Exit(1);
  }
  scenes.setActiveInstance(first.get());
  expect_true("Hero present", isValid(first->findEntityByName("Hero")));
  expect_true("Sidekick absent", !isValid(first->findEntityByName("Sidekick")));

  const eastl::shared_ptr<SceneInstance> cached = scenes.loadScene(virtual_path);
  expect_true("loadScene returns cached instance", cached.get() == first.get());

  writeTextFile(disk, kSceneHeroAndSidekick);
  expect_true("reload from disk ok", scenes.reloadActiveFromDisk());
  SceneInstance* active = scenes.getActiveInstance();
  expect_true("reload swapped instance", active != first.get());
  expect_true("Hero still present",
              active && isValid(active->findEntityByName("Hero")));
  expect_true("Sidekick from disk",
              active && isValid(active->findEntityByName("Sidekick")));

  writeTextFile(disk, "");
  SceneInstance* before_fail = scenes.getActiveInstance();
  expect_true("reload empty file fails", !scenes.reloadActiveFromDisk());
  expect_true("failed reload keeps world",
              scenes.getActiveInstance() == before_fail);
  expect_true("failed reload keeps Sidekick",
              before_fail && isValid(before_fail->findEntityByName("Sidekick")));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    std::fflush(stderr);
    std::_Exit(1);
  }
  std::printf("play_reload_scene_test: all passed\n");
  std::fflush(stdout);
  // Reload leaves two SceneInstance lifetimes; spdlog async flush during
  // their dtors races the logger thread pool. Skip process-wide teardown.
  std::_Exit(0);
}
