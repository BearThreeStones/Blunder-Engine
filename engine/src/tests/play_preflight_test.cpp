#include "runtime/project/play_preflight.h"
#include "runtime/project/play_session_controller.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void writeText(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

fs::path makeTempProject(const char* tag) {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_play_preflight_" + std::string(tag) + "_" +
       std::to_string(static_cast<unsigned long long>(stamp)));
  fs::create_directories(root / "Scripts");
  fs::create_directories(root / ".blunder" / "scripts_bin");
  writeText(root / "project.blunder", "name: PreflightTest\n");
  return root;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const PlayDirtySceneDecision clean =
        decidePlayDirtyScene(false, PlayDirtySceneChoice::Cancel);
    expect_true("clean scene proceeds", clean.proceed);
    expect_true("clean scene no save", !clean.save_first);
    expect_true("clean scene no prompt", !clean.needs_prompt);
  }

  {
    const PlayDirtySceneDecision need =
        decidePlayDirtyScene(true, std::nullopt);
    expect_true("dirty without choice needs prompt", need.needs_prompt);
    expect_true("dirty without choice does not proceed", !need.proceed);
  }

  {
    const PlayDirtySceneDecision save =
        decidePlayDirtyScene(true, PlayDirtySceneChoice::SaveAndPlay);
    expect_true("save-and-play proceeds", save.proceed);
    expect_true("save-and-play saves first", save.save_first);
    expect_true("save-and-play no prompt", !save.needs_prompt);
  }

  {
    const PlayDirtySceneDecision last =
        decidePlayDirtyScene(true, PlayDirtySceneChoice::PlayLastSaved);
    expect_true("play-last-saved proceeds", last.proceed);
    expect_true("play-last-saved no save", !last.save_first);
  }

  {
    const PlayDirtySceneDecision cancel =
        decidePlayDirtyScene(true, PlayDirtySceneChoice::Cancel);
    expect_true("cancel aborts", !cancel.proceed);
    expect_true("cancel no save", !cancel.save_first);
    expect_true("cancel no prompt", !cancel.needs_prompt);
  }

  {
    int build_calls = 0;
    PlayScriptsGateHooks hooks;
    hooks.is_dirty = []() { return false; };
    hooks.build = [&](std::string&) {
      ++build_calls;
      return true;
    };
    const PlayScriptsGateResult clean = runPlayScriptsGate(hooks);
    expect_true("clean scripts gate ok", clean.ok);
    expect_true("clean scripts skips build", !clean.build_invoked);
    expect_true("clean scripts build not called", build_calls == 0);
  }

  {
    int build_calls = 0;
    PlayScriptsGateHooks hooks;
    hooks.is_dirty = []() { return true; };
    hooks.build = [&](std::string& err) {
      ++build_calls;
      err = "compile failed";
      return false;
    };
    const PlayScriptsGateResult failed = runPlayScriptsGate(hooks);
    expect_true("dirty scripts build invoked", failed.build_invoked);
    expect_true("failed build not ok", !failed.ok);
    expect_true("failed build called once", build_calls == 1);
    expect_true("failed build error set",
                failed.error.find("compile") != std::string::npos);
  }

  {
    int build_calls = 0;
    PlayScriptsGateHooks hooks;
    hooks.is_dirty = []() { return true; };
    hooks.build = [&](std::string&) {
      ++build_calls;
      return true;
    };
    const PlayScriptsGateResult ok = runPlayScriptsGate(hooks);
    expect_true("dirty scripts build ok", ok.ok);
    expect_true("dirty scripts build invoked", ok.build_invoked);
    expect_true("dirty scripts build once", build_calls == 1);
  }

  {
    const fs::path project = makeTempProject("nodll");
    writeText(project / "Scripts" / "Game.csproj",
              "<Project Sdk=\"Microsoft.NET.Sdk\"></Project>\n");
    writeText(project / "Scripts" / "Hello.cs", "class Hello {}\n");
    expect_true("missing dll means dirty", areProjectScriptsDirty(project));
    fs::remove_all(project);
  }

  {
    const fs::path project = makeTempProject("stale");
    writeText(project / "Scripts" / "Game.csproj",
              "<Project Sdk=\"Microsoft.NET.Sdk\"></Project>\n");
    writeText(project / ".blunder" / "scripts_bin" / "Game.dll", "dll");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    writeText(project / "Scripts" / "Hello.cs", "class Hello {}\n");
    expect_true("newer source means dirty", areProjectScriptsDirty(project));
    fs::remove_all(project);
  }

  {
    const fs::path project = makeTempProject("fresh");
    writeText(project / "Scripts" / "Game.csproj",
              "<Project Sdk=\"Microsoft.NET.Sdk\"></Project>\n");
    writeText(project / "Scripts" / "Hello.cs", "class Hello {}\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    writeText(project / ".blunder" / "scripts_bin" / "Game.dll", "dll");
    expect_true("fresh dll not dirty", !areProjectScriptsDirty(project));
    fs::remove_all(project);
  }

  {
    struct FakeSession {
      bool process_running{false};
      bool spawn_ok{true};
      int spawn_count{0};
      int build_count{0};
      bool scripts_dirty{true};
      bool build_ok{false};
      std::vector<PlayIpcCommand> sent;
      PlaySpawnArgs last_spawn{};
      PlayIpcEndpoint endpoint{};
    } fake;
    fake.endpoint.ok = true;
    fake.endpoint.host = "127.0.0.1";
    fake.endpoint.port = 4242;

    PlaySessionHooks hooks;
    hooks.resolve_player = []() {
      return fs::path("engine_player.exe");
    };
    hooks.allocate_endpoint = [&]() { return fake.endpoint; };
    hooks.spawn = [&](const PlaySpawnArgs& args) {
      ++fake.spawn_count;
      fake.last_spawn = args;
      if (!fake.spawn_ok) {
        return false;
      }
      fake.process_running = true;
      return true;
    };
    hooks.is_process_running = [&]() { return fake.process_running; };
    hooks.terminate_process = [&]() { fake.process_running = false; };
    hooks.ipc_connect = [&](const PlayIpcEndpoint&) { return true; };
    hooks.ipc_wait_ready = [&](int) { return true; };
    hooks.ipc_send = [&](PlayIpcCommand cmd) {
      fake.sent.push_back(cmd);
      return true;
    };
    hooks.ipc_close = [&]() {};
    hooks.is_scripts_dirty = [&]() { return fake.scripts_dirty; };
    hooks.build_scripts = [&](std::string& err) {
      ++fake.build_count;
      if (!fake.build_ok) {
        err = "dotnet build failed";
        return false;
      }
      return true;
    };

    PlaySessionController ctrl(hooks);
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "assets/Scenes/a.scene.asset";
    expect_true("failed scripts gate returns false", !ctrl.play(req));
    expect_true("failed scripts gate stays stopped",
                ctrl.state() == PlaySessionState::Stopped);
    expect_true("failed scripts gate does not spawn", fake.spawn_count == 0);
    expect_true("failed scripts gate built once", fake.build_count == 1);
    expect_true("failed scripts gate has error",
                ctrl.lastError().find("dotnet") != std::string::npos);

    fake.build_ok = true;
    expect_true("successful scripts gate plays", ctrl.play(req));
    expect_true("successful scripts gate spawned", fake.spawn_count == 1);
    expect_true("successful scripts gate starting",
                ctrl.state() == PlaySessionState::Starting);
    expect_true("successful scripts gate built again", fake.build_count == 2);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "play_preflight_test: all passed\n");
  return 0;
}
