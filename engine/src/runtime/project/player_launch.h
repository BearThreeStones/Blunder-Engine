#pragma once

#include <filesystem>
#include <string>

namespace Blunder {

struct PlayerLaunch {
  bool ok{false};
  std::filesystem::path project_root;
  std::string scene;
  std::string play_ipc;
  bool headless{false};
  std::string error;
};

/// Parses `engine_player` CLI: `--project-root`, `--scene`, optional
/// `--play-ipc`, optional `--headless`.
PlayerLaunch parsePlayerLaunch(int argc, char** argv);

}  // namespace Blunder
