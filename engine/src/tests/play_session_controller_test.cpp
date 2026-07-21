#include "runtime/project/play_session_controller.h"

#include <cstdio>
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

struct FakeSession {
  bool process_running{false};
  bool spawn_ok{true};
  int spawn_count{0};
  int terminate_count{0};
  bool connected{false};
  bool ready{false};
  bool connect_ok{true};
  bool wait_ready_ok{true};
  std::vector<Blunder::PlayIpcCommand> sent;
  Blunder::PlaySpawnArgs last_spawn{};
  Blunder::PlayIpcEndpoint endpoint{};
};

Blunder::PlaySessionHooks makeFakeHooks(FakeSession& fake) {
  using namespace Blunder;
  fake.endpoint.ok = true;
  fake.endpoint.host = "127.0.0.1";
  fake.endpoint.port = 4242;

  PlaySessionHooks hooks;
  hooks.resolve_player = []() {
    return std::filesystem::path("engine_player.exe");
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
  hooks.terminate_process = [&]() {
    ++fake.terminate_count;
    fake.process_running = false;
  };
  hooks.ipc_connect = [&](const PlayIpcEndpoint&) {
    if (!fake.connect_ok) {
      return false;
    }
    fake.connected = true;
    return true;
  };
  hooks.ipc_wait_ready = [&](int) {
    if (!fake.connected || !fake.wait_ready_ok) {
      return false;
    }
    fake.ready = true;
    return true;
  };
  hooks.ipc_send = [&](PlayIpcCommand cmd) {
    fake.sent.push_back(cmd);
    return true;
  };
  hooks.ipc_close = [&]() {
    fake.connected = false;
    fake.ready = false;
  };
  return hooks;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    PlaySpawnArgs args;
    args.exe = "engine_player";
    args.project_root = "C:/Games/Demo";
    args.scene = "assets/Scenes/root.scene.asset";
    args.play_ipc = "127.0.0.1:5555";
    const auto argv = buildPlayerSpawnArgv(args);
    expect_true("argv size", argv.size() == 7);
    expect_true("argv --project-root", argv[1] == "--project-root");
    expect_true("argv project path", argv[2] == "C:/Games/Demo");
    expect_true("argv --scene", argv[3] == "--scene");
    expect_true("argv scene", argv[4] == "assets/Scenes/root.scene.asset");
    expect_true("argv --play-ipc", argv[5] == "--play-ipc");
    expect_true("argv ipc", argv[6] == "127.0.0.1:5555");
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    expect_true("starts stopped", ctrl.state() == PlaySessionState::Stopped);
    expect_true("pause disabled when stopped", !ctrl.pauseEnabled());
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "assets/Scenes/a.scene.asset";
    expect_true("play ok", ctrl.play(req));
    expect_true("state starting", ctrl.state() == PlaySessionState::Starting);
    expect_true("spawned once", fake.spawn_count == 1);
    expect_true("spawn scene", fake.last_spawn.scene == req.scene);
    expect_true("spawn ipc", fake.last_spawn.play_ipc == "127.0.0.1:4242");
    expect_true("pause disabled while starting", !ctrl.pauseEnabled());

    ctrl.poll();
    expect_true("ready after poll", ctrl.isReady());
    expect_true("state playing", ctrl.state() == PlaySessionState::Playing);
    expect_true("pause enabled after ready", ctrl.pauseEnabled());
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play for pause", ctrl.play(req));
    ctrl.poll();
    expect_true("pause ok", ctrl.pause());
    expect_true("state paused", ctrl.state() == PlaySessionState::Paused);
    expect_true("sent pause", !fake.sent.empty() &&
                                  fake.sent.back() == PlayIpcCommand::Pause);
    expect_true("resume ok", ctrl.resume());
    expect_true("state playing after resume",
                ctrl.state() == PlaySessionState::Playing);
    expect_true("sent resume", fake.sent.back() == PlayIpcCommand::Resume);
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play for stop", ctrl.play(req));
    ctrl.poll();
    expect_true("stop ok", ctrl.stop());
    expect_true("stopped", ctrl.state() == PlaySessionState::Stopped);
    expect_true("sent stop", !fake.sent.empty() &&
                                 fake.sent.back() == PlayIpcCommand::Stop);
    expect_true("terminated", fake.terminate_count >= 1);
    expect_true("pause disabled after stop", !ctrl.pauseEnabled());
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play before exit", ctrl.play(req));
    ctrl.poll();
    expect_true("playing before exit",
                ctrl.state() == PlaySessionState::Playing);
    fake.process_running = false;
    ctrl.poll();
    expect_true("exit -> stopped", ctrl.state() == PlaySessionState::Stopped);
    expect_true("not ready after exit", !ctrl.isReady());
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "first";
    expect_true("first play", ctrl.play(req));
    ctrl.poll();
    req.scene = "second";
    expect_true("second play", ctrl.play(req));
    expect_true("single-session respawn", fake.spawn_count == 2);
    expect_true("stopped previous", fake.terminate_count >= 1);
    expect_true("new scene", fake.last_spawn.scene == "second");
    expect_true("starting after respawn",
                ctrl.state() == PlaySessionState::Starting);
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play before early pause", ctrl.play(req));
    expect_true("still starting", ctrl.state() == PlaySessionState::Starting);
    expect_true("pause rejected before ready", !ctrl.pause());
    expect_true("no pause cmd", fake.sent.empty());
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "play_session_controller_test: all passed\n");
  return 0;
}
