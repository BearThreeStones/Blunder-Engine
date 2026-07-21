#pragma once

#include "runtime/project/play_ipc.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Blunder {

enum class PlaySessionState : uint8_t {
  Stopped = 0,
  Starting,
  Playing,
  Paused,
};

struct PlaySessionRequest {
  std::filesystem::path project_root;
  std::string scene;
};

struct PlaySpawnArgs {
  std::filesystem::path exe;
  std::filesystem::path project_root;
  std::string scene;
  std::string play_ipc;
};

/// Argv for `engine_player`: exe, --project-root, path, --scene, scene,
/// --play-ipc, endpoint.
std::vector<std::string> buildPlayerSpawnArgv(const PlaySpawnArgs& args);

/// Sibling `engine_player` next to the current executable.
std::filesystem::path resolvePlayerExecutablePath();

struct PlaySessionHooks {
  std::function<std::filesystem::path()> resolve_player;
  std::function<PlayIpcEndpoint()> allocate_endpoint;
  std::function<bool(const PlaySpawnArgs&)> spawn;
  std::function<bool()> is_process_running;
  std::function<void()> terminate_process;
  std::function<bool(const PlayIpcEndpoint&)> ipc_connect;
  std::function<bool(int timeout_ms)> ipc_wait_ready;
  std::function<bool(PlayIpcCommand)> ipc_send;
  std::function<void()> ipc_close;
  /// Optional Scripts dirty gate (run before spawn). When unset, gate is skipped.
  std::function<bool()> is_scripts_dirty;
  std::function<bool(std::string& error)> build_scripts;
  /// Starting to Playing deadline. Default 15s.
  std::chrono::milliseconds starting_timeout{std::chrono::milliseconds(15000)};
  /// Optional clock for tests; defaults to steady_clock::now.
  std::function<std::chrono::steady_clock::time_point()> now;
};

/// Editor Play session: spawn Player, IPC pause/resume/stop, track exit.
class PlaySessionController final {
 public:
  PlaySessionController();
  explicit PlaySessionController(PlaySessionHooks hooks);
  ~PlaySessionController();

  PlaySessionController(const PlaySessionController&) = delete;
  PlaySessionController& operator=(const PlaySessionController&) = delete;

  PlaySessionState state() const { return m_state; }
  bool isReady() const { return m_ready; }
  bool pauseEnabled() const;
  bool stopEnabled() const { return m_state != PlaySessionState::Stopped; }

  const PlayIpcEndpoint& endpoint() const { return m_endpoint; }
  const std::string& lastError() const { return m_last_error; }

  /// Surface a non-session error (e.g. Save and Play save failure) without
  /// starting Play.
  void setLastError(std::string error);

  /// Install Scripts dirty/build hooks used by `play()` before spawn.
  void setScriptsPreflight(std::function<bool()> is_dirty,
                           std::function<bool(std::string& error)> build);

  bool play(const PlaySessionRequest& request);
  bool pause();
  bool resume();
  bool stop();
  void poll();

  static PlaySessionHooks makeDefaultHooks();

 private:
  void resetToStopped();
  void onProcessGone();
  void failStarting(std::string error);
  std::chrono::steady_clock::time_point now() const;

  PlaySessionHooks m_hooks;
  PlaySessionState m_state{PlaySessionState::Stopped};
  bool m_ready{false};
  bool m_ipc_connected{false};
  PlayIpcEndpoint m_endpoint;
  std::string m_last_error;
  bool m_has_starting_deadline{false};
  std::chrono::steady_clock::time_point m_starting_deadline{};
};

}  // namespace Blunder
