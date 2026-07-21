#include "runtime/project/player_launch.h"

#include <cstring>
#include <utility>

namespace Blunder {

PlayerLaunch parsePlayerLaunch(int argc, char** argv) {
  PlayerLaunch options;
  std::filesystem::path project_root;
  std::string scene;
  std::string play_ipc;

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg == nullptr) {
      continue;
    }
    if (std::strcmp(arg, "--project-root") == 0) {
      if (i + 1 < argc && argv[i + 1] != nullptr) {
        project_root = argv[++i];
      }
      continue;
    }
    if (std::strcmp(arg, "--scene") == 0) {
      if (i + 1 < argc && argv[i + 1] != nullptr) {
        scene = argv[++i];
      }
      continue;
    }
    if (std::strcmp(arg, "--play-ipc") == 0) {
      if (i + 1 < argc && argv[i + 1] != nullptr) {
        play_ipc = argv[++i];
      }
      continue;
    }
  }

  if (project_root.empty()) {
    options.ok = false;
    options.error =
        "Missing --project-root <path>. Launch engine_player with a Project "
        "root.";
    return options;
  }
  if (scene.empty()) {
    options.ok = false;
    options.error =
        "Missing --scene <virtual-path-or-guid>. Pass the Play entry scene "
        "asset.";
    return options;
  }

  options.ok = true;
  options.project_root = std::move(project_root);
  options.scene = std::move(scene);
  options.play_ipc = std::move(play_ipc);
  return options;
}

}  // namespace Blunder
