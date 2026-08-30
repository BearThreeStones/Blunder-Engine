#pragma once

#include "runtime/project/authorship_issue.h"
#include "runtime/project/play_ipc.h"
#include "runtime/project/play_pose_preview.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "EASTL/vector.h"

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
  std::string scene_guid;
  bool headless{false};
};

struct PlaySpawnArgs {
  std::filesystem::path exe;
  std::filesystem::path project_root;
  std::string scene;
  std::string play_ipc;
  bool headless{false};
};

/// Argv for `engine_player`: exe, --project-root, path, --scene, scene,
/// --play-ipc, endpoint, optional `--headless`.
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
  /// After handshake: drain Player NDJSON logs. Unset skips ingest (tests).
  std::function<std::vector<PlayIpcLogRecord>()> ipc_poll_logs;
  std::function<bool(uint32_t ticks)> ipc_send_step;
  std::function<bool()> ipc_send_frame;
  std::function<std::vector<PlayIpcFrameRecord>()> ipc_poll_frames;
  std::function<std::vector<PlayIpcErrorRecord>()> ipc_poll_errors;
  std::function<std::vector<PlayIpcIssueRecord>()> ipc_poll_issues;
  std::function<std::vector<PlayIpcReloadRecord>()> ipc_poll_reloads;
  std::function<std::vector<PlayIpcPosesRecord>()> ipc_poll_poses;
  std::function<bool(const std::string& json)> ipc_send_patch;
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
  bool reloadEnabled() const { return pauseEnabled(); }
  bool stopEnabled() const { return m_state != PlaySessionState::Stopped; }

  const PlayIpcEndpoint& endpoint() const { return m_endpoint; }
  const std::string& lastError() const { return m_last_error; }
  const std::string& lastRequestFailure() const { return m_last_request_failure; }
  const eastl::vector<Issue>& lastIssues() const { return m_last_issues; }
  const PlayIpcFrameRecord& lastPlayFrame() const { return m_last_play_frame; }
  const std::string& playEntryScene() const { return m_play_entry_scene; }
  const std::string& playEntryGuid() const { return m_play_entry_guid; }
  const PlayPoseOverlayMap& poseOverlay() const { return m_pose_overlay; }

  /// Surface a non-session error (e.g. Save and Play save failure) without
  /// starting Play.
  void setLastError(std::string error);

  void setLastIssues(eastl::vector<Issue> issues);

  /// Install Scripts dirty/build hooks used by `play()` before spawn.
  void setScriptsPreflight(std::function<bool()> is_dirty,
                           std::function<bool(std::string& error)> build);

  bool clearOnPlay() const;
  void setClearOnPlay(bool value);
  bool errorPause() const;
  void setErrorPause(bool value);

  bool play(const PlaySessionRequest& request);
  bool pause();
  bool resume();
  bool stop();
  bool reload();
  bool sendPatch(const std::string& json);
  bool step(uint32_t ticks);
  bool requestPlayFrame();
  /// Send Play frame then poll until a frame arrives or `timeout_ms` elapses.
  /// Windowed GUI still uses one-shot `requestPlayFrame`. `pump` runs each
  /// wait iteration so Headless adapters can tick while IPC arrives.
  bool waitForPlayFrame(int timeout_ms, std::function<void()> pump = {});
  void poll();

  static PlaySessionHooks makeDefaultHooks();

 private:
  void resetToStopped();
  void onProcessGone();
  void failStarting(std::string error);
  void ingestPlayLogs();
  void ingestPlayFrames();
  void ingestPlayIssues();
  void ingestPlayReloads();
  void ingestPlayPoses();
  std::chrono::steady_clock::time_point now() const;

  PlaySessionHooks m_hooks;
  PlaySessionState m_state{PlaySessionState::Stopped};
  bool m_ready{false};
  bool m_ipc_connected{false};
  PlayIpcEndpoint m_endpoint;
  std::string m_last_error;
  std::string m_last_request_failure;
  PlayIpcFrameRecord m_last_play_frame;
  eastl::vector<Issue> m_last_issues;
  std::string m_play_entry_scene;
  std::string m_play_entry_guid;
  PlayPoseOverlayMap m_pose_overlay;
  bool m_has_starting_deadline{false};
  std::chrono::steady_clock::time_point m_starting_deadline{};
};

}  // namespace Blunder
