#include "runtime/project/play_session_controller.h"
#include "runtime/core/log/console_ring.h"
#include "runtime/project/authorship_issue.h"
#include "runtime/project/play_step.h"

#include <chrono>
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
  std::vector<Blunder::PlayIpcLogRecord> pending_logs;
  std::vector<Blunder::PlayIpcFrameRecord> pending_frames;
  uint32_t last_step_ticks{0};
  int step_sends{0};
  int frame_sends{0};
  int polls_until_frame{0};
  bool frame_queued{false};
  Blunder::PlayIpcFrameRecord queued_frame{};
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
  hooks.ipc_poll_logs = [&]() {
    std::vector<PlayIpcLogRecord> out;
    out.swap(fake.pending_logs);
    return out;
  };
  hooks.ipc_send_step = [&](uint32_t ticks) {
    ++fake.step_sends;
    fake.last_step_ticks = ticks;
    return true;
  };
  hooks.ipc_send_frame = [&]() {
    ++fake.frame_sends;
    PlayIpcFrameRecord frame;
    frame.width = 16;
    frame.height = 9;
    frame.encoding = "rgba8";
    frame.rgba.assign(16u * 9u * 4u, 3);
    if (fake.polls_until_frame == 0) {
      fake.pending_frames.push_back(std::move(frame));
    } else {
      fake.frame_queued = true;
      fake.queued_frame = std::move(frame);
    }
    return true;
  };
  hooks.ipc_poll_frames = [&]() {
    if (fake.polls_until_frame > 0) {
      --fake.polls_until_frame;
      return std::vector<PlayIpcFrameRecord>{};
    }
    if (fake.frame_queued) {
      fake.frame_queued = false;
      std::vector<PlayIpcFrameRecord> out;
      out.push_back(std::move(fake.queued_frame));
      return out;
    }
    std::vector<PlayIpcFrameRecord> out;
    out.swap(fake.pending_frames);
    return out;
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
    PlaySpawnArgs args;
    args.exe = "engine_player";
    args.project_root = "C:/Games/Demo";
    args.scene = "assets/Scenes/root.scene.asset";
    args.play_ipc = "127.0.0.1:5555";
    args.headless = true;
    const auto argv = buildPlayerSpawnArgv(args);
    expect_true("headless argv size", argv.size() == 8);
    expect_true("headless argv flag", argv[7] == "--headless");
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
    expect_true("windowed spawn", !fake.last_spawn.headless);
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

  {
    FakeSession fake;
    fake.wait_ready_ok = false;
    auto hooks = makeFakeHooks(fake);
    hooks.starting_timeout = std::chrono::milliseconds(100);
    const auto t0 = std::chrono::steady_clock::now();
    auto offset = std::chrono::milliseconds(0);
    hooks.now = [&]() { return t0 + offset; };
    PlaySessionController ctrl(std::move(hooks));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play for timeout", ctrl.play(req));
    expect_true("starting before timeout",
                ctrl.state() == PlaySessionState::Starting);
    offset = std::chrono::milliseconds(150);
    ctrl.poll();
    expect_true("timeout -> stopped",
                ctrl.state() == PlaySessionState::Stopped);
    expect_true("timeout terminated", fake.terminate_count >= 1);
    expect_true("timeout error set",
                ctrl.lastError().find("timeout") != std::string::npos);
  }

  {
    FakeSession fake;
    auto hooks = makeFakeHooks(fake);
    hooks.is_scripts_dirty = []() { return true; };
    hooks.build_scripts = [](std::string& err) {
      err = "dotnet build failed";
      return false;
    };
    PlaySessionController ctrl(std::move(hooks));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("scripts fail no play", !ctrl.play(req));
    expect_true("scripts fail stopped",
                ctrl.state() == PlaySessionState::Stopped);
    expect_true("scripts fail no spawn", fake.spawn_count == 0);
    expect_true("scripts fail issue",
                issueListHasCode(ctrl.lastIssues(),
                                 k_issue_scripts_build_failed));
  }

  {
    consoleViewSettings() = ConsoleViewSettings{};
    ConsoleRing::instance().clear();
    ConsoleRing::instance().append(ConsoleSeverity::Log,
                                   ConsoleOrigin::EditorSession, "keep-me");
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    expect_true("clear on play default", consoleViewSettings().clear_on_play);
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play clears ring", ctrl.play(req));
    expect_true("ring cleared on play", ConsoleRing::instance().size() == 0);

    consoleViewSettings().clear_on_play = false;
    ConsoleRing::instance().append(ConsoleSeverity::Log,
                                   ConsoleOrigin::EditorSession, "stay");
    expect_true("play again without clear", ctrl.play(req));
    expect_true("ring kept when clear off", ConsoleRing::instance().size() == 1);
  }

  {
    consoleViewSettings() = ConsoleViewSettings{};
    consoleViewSettings().clear_on_play = false;
    consoleViewSettings().error_pause = true;
    ConsoleRing::instance().clear();
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play for error pause", ctrl.play(req));
    ctrl.poll();
    expect_true("playing before play error",
                ctrl.state() == PlaySessionState::Playing);

    PlayIpcLogRecord err;
    err.sev = "error";
    err.text = "play-error";
    fake.pending_logs.push_back(err);
    ctrl.poll();
    expect_true("error pause paused", ctrl.state() == PlaySessionState::Paused);
    expect_true("error pause sent pause",
                !fake.sent.empty() && fake.sent.back() == PlayIpcCommand::Pause);
    expect_true("play error ingested", ConsoleRing::instance().size() >= 1);
    const auto snap = ConsoleRing::instance().snapshot();
    expect_true("play origin", !snap.empty() &&
                                   snap.back().origin == ConsoleOrigin::PlayProcess);

    ConsoleRing::instance().clear();
    ctrl.resume();
    expect_true("resumed", ctrl.state() == PlaySessionState::Playing);
    ConsoleRing::instance().append(ConsoleSeverity::Error,
                                   ConsoleOrigin::EditorSession, "editor-err");
    ctrl.poll();
    expect_true("editor error does not pause",
                ctrl.state() == PlaySessionState::Playing);

    expect_true("stop keeps ring", ctrl.stop());
    expect_true("ring after stop", ConsoleRing::instance().size() == 1);
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play for step", ctrl.play(req));
    ctrl.poll();
    expect_true("playing rejects step", !ctrl.step(30));
    expect_true("step requires pause code",
                ctrl.lastRequestFailure() == k_request_play_step_requires_pause);
    expect_true("playing step not sent", fake.step_sends == 0);
    expect_true("pause for step", ctrl.pause());
    expect_true("paused step ok", ctrl.step(30));
    expect_true("stays paused after step",
                ctrl.state() == PlaySessionState::Paused);
    expect_true("step sent once", fake.step_sends == 1);
    expect_true("step ticks 30", fake.last_step_ticks == 30);
    expect_true("frame after step", ctrl.requestPlayFrame());
    expect_true("frame 16:9", ctrl.lastPlayFrame().width == 16 &&
                                  ctrl.lastPlayFrame().height == 9);
    expect_true("frame not square",
                ctrl.lastPlayFrame().width != ctrl.lastPlayFrame().height);
    expect_true("frame send once", fake.frame_sends == 1);
  }

  {
    FakeSession fake;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    req.headless = true;
    expect_true("headless play", ctrl.play(req));
    expect_true("spawn headless", fake.last_spawn.headless);
  }

  {
    FakeSession fake;
    fake.polls_until_frame = 2;
    PlaySessionController ctrl(makeFakeHooks(fake));
    PlaySessionRequest req;
    req.project_root = "C:/proj";
    req.scene = "scene";
    expect_true("play for wait frame", ctrl.play(req));
    ctrl.poll();
    expect_true("pause for wait frame", ctrl.pause());
    expect_true("one-shot empty while delayed", !ctrl.requestPlayFrame());
    fake.polls_until_frame = 2;
    fake.frame_queued = false;
    expect_true("wait on poll", ctrl.waitForPlayFrame(1000));
    expect_true("waited frame 16:9", ctrl.lastPlayFrame().width == 16 &&
                                         ctrl.lastPlayFrame().height == 9);
  }

  consoleViewSettings() = ConsoleViewSettings{};
  ConsoleRing::instance().clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "play_session_controller_test: all passed\n");
  return 0;
}
