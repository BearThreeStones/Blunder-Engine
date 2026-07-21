#include "runtime/project/player_launch.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

std::vector<char*> makeArgv(std::vector<std::string>& storage) {
  std::vector<char*> argv;
  argv.reserve(storage.size());
  for (std::string& s : storage) {
    argv.push_back(s.data());
  }
  return argv;
}

}  // namespace

int main() {
  using namespace Blunder;
  namespace fs = std::filesystem;

  {
    std::vector<std::string> args = {"engine_player"};
    auto argv = makeArgv(args);
    const PlayerLaunch opts =
        parsePlayerLaunch(static_cast<int>(argv.size()), argv.data());
    expect_true("no-args fails", !opts.ok);
    expect_true("error mentions project-root",
                opts.error.find("project-root") != std::string::npos);
  }

  {
    std::vector<std::string> args = {"engine_player", "--project-root",
                                     "C:/Games/Demo"};
    auto argv = makeArgv(args);
    const PlayerLaunch opts =
        parsePlayerLaunch(static_cast<int>(argv.size()), argv.data());
    expect_true("project-root alone fails without scene", !opts.ok);
    expect_true("error mentions scene",
                opts.error.find("scene") != std::string::npos);
  }

  {
    std::vector<std::string> args = {
        "engine_player", "--project-root", "C:/Games/Demo", "--scene",
        "assets/Scenes/main.scene.asset"};
    auto argv = makeArgv(args);
    const PlayerLaunch opts =
        parsePlayerLaunch(static_cast<int>(argv.size()), argv.data());
    expect_true("project+scene ok", opts.ok);
    expect_true("root path set",
                opts.project_root == fs::path("C:/Games/Demo"));
    expect_true("scene set",
                opts.scene == "assets/Scenes/main.scene.asset");
    expect_true("play-ipc empty by default", opts.play_ipc.empty());
  }

  {
    std::vector<std::string> args = {
        "engine_player",
        "--project-root",
        "C:/Games/Demo",
        "--scene",
        "assets/Scenes/main.scene.asset",
        "--play-ipc",
        "127.0.0.1:54321"};
    auto argv = makeArgv(args);
    const PlayerLaunch opts =
        parsePlayerLaunch(static_cast<int>(argv.size()), argv.data());
    expect_true("full cli ok", opts.ok);
    expect_true("play-ipc set", opts.play_ipc == "127.0.0.1:54321");
  }

  {
    std::vector<std::string> args = {
        "engine_player", "--scene", "assets/Scenes/main.scene.asset",
        "--project-root", "D:/Proj", "--play-ipc", "pipe:play"};
    auto argv = makeArgv(args);
    const PlayerLaunch opts =
        parsePlayerLaunch(static_cast<int>(argv.size()), argv.data());
    expect_true("flag order ok", opts.ok);
    expect_true("root after scene", opts.project_root == fs::path("D:/Proj"));
    expect_true("scene before root",
                opts.scene == "assets/Scenes/main.scene.asset");
    expect_true("ipc last", opts.play_ipc == "pipe:play");
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
