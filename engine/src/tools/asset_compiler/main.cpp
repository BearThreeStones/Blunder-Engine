#include "runtime/core/log/log_system.h"
#include "runtime/engine.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <chrono>
#include <cstring>

namespace {

void printUsage() {
  std::fprintf(stderr,
               "Usage: asset_compiler --project-root <path> [--force] "
               "[--player-trust]\n"
               "  Default: cookIfStale (verify stamp fingerprint).\n"
               "  --player-trust: cookIfStaleForPlayer (verify fingerprint; "
               "skip cook walk on match).\n"
               "  --force: cookAll(true) then refresh cook stamp on success.\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path project_root;
  bool force = false;
  bool player_trust = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--project-root") == 0 && i + 1 < argc) {
      project_root = argv[++i];
    } else if (std::strcmp(argv[i], "--force") == 0) {
      force = true;
    } else if (std::strcmp(argv[i], "--player-trust") == 0) {
      player_trust = true;
    } else if (std::strcmp(argv[i], "--help") == 0 ||
               std::strcmp(argv[i], "-h") == 0) {
      printUsage();
      return 0;
    }
  }

  if (project_root.empty()) {
    printUsage();
    return 1;
  }

  Blunder::g_runtime_global_context.m_memory_system.initialize();
  Blunder::g_runtime_global_context.m_logger_system =
      eastl::make_shared<Blunder::LogSystem>();

  auto file_system = eastl::make_shared<Blunder::FileSystem>();
  Blunder::FileSystemInitInfo fs_init{};
  fs_init.project_root = project_root;
  file_system->initialize(fs_init);

  auto asset_manager = eastl::make_shared<Blunder::AssetManager>();
  Blunder::AssetManagerInitInfo asset_init{};
  asset_init.file_system = file_system.get();
  asset_manager->initialize(asset_init);

  Blunder::AssetRegistry registry;
  registry.initialize(file_system.get());

  Blunder::AssetCompilerService compiler;
  compiler.initialize(file_system.get(), asset_manager.get(), &registry);

  const auto begin = std::chrono::steady_clock::now();
  Blunder::AssetCompilerStats stats{};
  if (force) {
    stats = compiler.cookAll(true);
    if (!stats.aborted && stats.failed == 0) {
      (void)compiler.refreshCookStamp();
    }
  } else if (player_trust) {
    stats = compiler.cookIfStaleForPlayer();
  } else {
    stats = compiler.cookIfStale();
  }
  const double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - begin)
                        .count();

  std::fprintf(stdout,
               "asset_compiler: meshes=%u textures=%u skipped=%u failed=%u "
               "used_stamp=%d aborted=%d wall_ms=%.1f\n",
               stats.meshes_cooked, stats.textures_cooked, stats.skipped,
               stats.failed, stats.used_stamp ? 1 : 0, stats.aborted ? 1 : 0,
               ms);

  compiler.shutdown();
  registry.shutdown();
  asset_manager->shutdown();
  file_system->shutdown();
  Blunder::g_runtime_global_context.m_logger_system.reset();
  Blunder::g_runtime_global_context.m_memory_system.shutdown();

  return stats.failed > 0 ? 2 : 0;
}
