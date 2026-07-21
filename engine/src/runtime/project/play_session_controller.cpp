#include "runtime/project/play_session_controller.h"

#include <memory>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <spawn.h>
extern char** environ;
#endif

namespace Blunder {
namespace {

std::filesystem::path queryExecutablePath() {
#ifdef _WIN32
  wchar_t buffer[MAX_PATH];
  const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }
  return std::filesystem::path(buffer);
#elif defined(__linux__)
  char buffer[PATH_MAX];
  const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (len <= 0) {
    return {};
  }
  buffer[len] = '\0';
  return std::filesystem::path(buffer);
#elif defined(__APPLE__)
  char buffer[PATH_MAX];
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    return {};
  }
  return std::filesystem::path(buffer);
#else
  return {};
#endif
}

std::filesystem::path playerExecutableName() {
#ifdef _WIN32
  return "engine_player.exe";
#else
  return "engine_player";
#endif
}

#ifdef _WIN32
std::wstring quoteWindowsArg(const std::wstring& arg) {
  if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
    return arg;
  }
  std::wstring out = L"\"";
  for (wchar_t c : arg) {
    if (c == L'"') {
      out += L"\\\"";
    } else {
      out += c;
    }
  }
  out += L'"';
  return out;
}
#endif

struct NativePlayProcess {
#ifdef _WIN32
  HANDLE process{nullptr};

  ~NativePlayProcess() { close(); }

  void close() {
    if (process != nullptr) {
      CloseHandle(process);
      process = nullptr;
    }
  }

  bool running() const {
    if (process == nullptr) {
      return false;
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(process, &code)) {
      return false;
    }
    return code == STILL_ACTIVE;
  }

  void terminate() {
    if (process != nullptr) {
      TerminateProcess(process, 1);
    }
  }
#else
  pid_t pid{0};

  bool running() const {
    if (pid <= 0) {
      return false;
    }
    int status = 0;
    const pid_t rc = waitpid(pid, &status, WNOHANG);
    if (rc == 0) {
      return true;
    }
    return false;
  }

  void terminate() {
    if (pid > 0) {
      kill(pid, SIGTERM);
    }
  }

  void close() { pid = 0; }
#endif
};

struct DefaultPlayRuntime {
  std::shared_ptr<NativePlayProcess> process =
      std::make_shared<NativePlayProcess>();
  std::shared_ptr<PlayIpcClient> client = std::make_shared<PlayIpcClient>();
};

bool spawnPlayerProcess(const PlaySpawnArgs& args,
                        NativePlayProcess& out_process) {
  if (args.exe.empty()) {
    return false;
  }

  PlaySpawnArgs spawn_args = args;
  const auto argv_storage = buildPlayerSpawnArgv(spawn_args);
  if (argv_storage.size() < 7) {
    return false;
  }

#ifdef _WIN32
  std::wstring command_line;
  for (size_t i = 0; i < argv_storage.size(); ++i) {
    if (i > 0) {
      command_line.push_back(L' ');
    }
    command_line +=
        quoteWindowsArg(std::filesystem::path(argv_storage[i]).wstring());
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::wstring mutable_cmd = command_line;
  const BOOL ok =
      CreateProcessW(args.exe.c_str(), mutable_cmd.data(), nullptr, nullptr,
                     FALSE, 0, nullptr, nullptr, &si, &pi);
  if (!ok) {
    return false;
  }
  CloseHandle(pi.hThread);
  out_process.close();
  out_process.process = pi.hProcess;
  return true;
#elif defined(__linux__) || defined(__APPLE__)
  std::vector<std::string> storage = argv_storage;
  storage[0] = args.exe.string();
  std::vector<char*> argv;
  argv.reserve(storage.size() + 1);
  for (std::string& arg : storage) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  pid_t pid = 0;
  const int rc = posix_spawn(&pid, args.exe.string().c_str(), nullptr, nullptr,
                             argv.data(), environ);
  if (rc != 0) {
    return false;
  }
  out_process.pid = pid;
  return true;
#else
  (void)out_process;
  return false;
#endif
}

PlayIpcEndpoint allocateEphemeralLoopbackEndpoint() {
  PlayIpcServer probe;
  PlayIpcEndpoint ep;
  ep.host = "127.0.0.1";
  ep.port = 0;
  ep.ok = true;
  if (!probe.listen(ep)) {
    ep.ok = false;
    ep.error = "failed to allocate ephemeral play-ipc port";
    return ep;
  }
  ep.port = probe.boundPort();
  probe.close();
  return ep;
}

}  // namespace

std::vector<std::string> buildPlayerSpawnArgv(const PlaySpawnArgs& args) {
  std::vector<std::string> argv;
  argv.reserve(7);
  argv.push_back(args.exe.empty() ? std::string("engine_player")
                                  : args.exe.generic_string());
  argv.push_back("--project-root");
  argv.push_back(args.project_root.generic_string());
  argv.push_back("--scene");
  argv.push_back(args.scene);
  argv.push_back("--play-ipc");
  argv.push_back(args.play_ipc);
  return argv;
}

std::filesystem::path resolvePlayerExecutablePath() {
  const std::filesystem::path self = queryExecutablePath();
  if (self.empty()) {
    return {};
  }
  const std::filesystem::path sibling =
      self.parent_path() / playerExecutableName();
  std::error_code ec;
  if (std::filesystem::exists(sibling, ec)) {
    return sibling;
  }

  // CMake VS multi-config layout stages editor/player in sibling folders:
  //   .../editor/Debug/engine_editor.exe
  //   .../player/Debug/engine_player.exe
  const std::filesystem::path config_dir = self.parent_path();
  const std::filesystem::path target_dir = config_dir.parent_path();
  if (target_dir.filename() == "editor") {
    const std::filesystem::path staged =
        target_dir.parent_path() / "player" / config_dir.filename() /
        playerExecutableName();
    if (std::filesystem::exists(staged, ec)) {
      return staged;
    }
  }
  return sibling;
}

PlaySessionHooks PlaySessionController::makeDefaultHooks() {
  auto runtime = std::make_shared<DefaultPlayRuntime>();

  PlaySessionHooks hooks;
  hooks.resolve_player = []() { return resolvePlayerExecutablePath(); };
  hooks.allocate_endpoint = []() { return allocateEphemeralLoopbackEndpoint(); };
  hooks.spawn = [runtime](const PlaySpawnArgs& args) {
    return spawnPlayerProcess(args, *runtime->process);
  };
  hooks.is_process_running = [runtime]() {
    return runtime->process->running();
  };
  hooks.terminate_process = [runtime]() {
    runtime->process->terminate();
    // Allow the OS a moment; Wait is optional for editor Stop.
    runtime->process->close();
  };
  hooks.ipc_connect = [runtime](const PlayIpcEndpoint& endpoint) {
    return runtime->client->connect(endpoint);
  };
  hooks.ipc_wait_ready = [runtime](int timeout_ms) {
    return runtime->client->waitReady(timeout_ms);
  };
  hooks.ipc_send = [runtime](PlayIpcCommand command) {
    return runtime->client->sendCommand(command);
  };
  hooks.ipc_close = [runtime]() { runtime->client->close(); };
  return hooks;
}

PlaySessionController::PlaySessionController()
    : PlaySessionController(makeDefaultHooks()) {}

PlaySessionController::PlaySessionController(PlaySessionHooks hooks)
    : m_hooks(std::move(hooks)) {}

PlaySessionController::~PlaySessionController() {
  if (m_state != PlaySessionState::Stopped) {
    stop();
  }
}

bool PlaySessionController::pauseEnabled() const {
  return m_ready && (m_state == PlaySessionState::Playing ||
                     m_state == PlaySessionState::Paused);
}

void PlaySessionController::resetToStopped() {
  if (m_hooks.ipc_close) {
    m_hooks.ipc_close();
  }
  m_state = PlaySessionState::Stopped;
  m_ready = false;
  m_ipc_connected = false;
  m_endpoint = {};
}

void PlaySessionController::onProcessGone() {
  resetToStopped();
  m_last_error.clear();
}

bool PlaySessionController::play(const PlaySessionRequest& request) {
  m_last_error.clear();
  if (request.project_root.empty() || request.scene.empty()) {
    m_last_error = "project root and scene are required";
    return false;
  }

  if (m_state != PlaySessionState::Stopped) {
    stop();
  }

  if (!m_hooks.resolve_player || !m_hooks.allocate_endpoint || !m_hooks.spawn) {
    m_last_error = "play session hooks incomplete";
    return false;
  }

  const std::filesystem::path exe = m_hooks.resolve_player();
  if (exe.empty()) {
    m_last_error = "could not resolve engine_player executable";
    return false;
  }

  PlayIpcEndpoint endpoint = m_hooks.allocate_endpoint();
  if (!endpoint.ok || endpoint.port == 0) {
    m_last_error = endpoint.error.empty() ? "failed to allocate play-ipc endpoint"
                                          : endpoint.error;
    return false;
  }

  PlaySpawnArgs spawn_args;
  spawn_args.exe = exe;
  spawn_args.project_root = request.project_root;
  spawn_args.scene = request.scene;
  spawn_args.play_ipc = formatPlayIpcEndpoint(endpoint);

  if (!m_hooks.spawn(spawn_args)) {
    m_last_error = "failed to spawn engine_player";
    return false;
  }

  m_endpoint = endpoint;
  m_ready = false;
  m_ipc_connected = false;
  m_state = PlaySessionState::Starting;
  return true;
}

bool PlaySessionController::pause() {
  if (!pauseEnabled() || m_state != PlaySessionState::Playing) {
    return false;
  }
  if (!m_hooks.ipc_send || !m_hooks.ipc_send(PlayIpcCommand::Pause)) {
    m_last_error = "failed to send pause";
    return false;
  }
  m_state = PlaySessionState::Paused;
  return true;
}

bool PlaySessionController::resume() {
  if (!pauseEnabled() || m_state != PlaySessionState::Paused) {
    return false;
  }
  if (!m_hooks.ipc_send || !m_hooks.ipc_send(PlayIpcCommand::Resume)) {
    m_last_error = "failed to send resume";
    return false;
  }
  m_state = PlaySessionState::Playing;
  return true;
}

bool PlaySessionController::stop() {
  if (m_state == PlaySessionState::Stopped) {
    return true;
  }

  if (m_ready && m_hooks.ipc_send) {
    m_hooks.ipc_send(PlayIpcCommand::Stop);
  }
  if (m_hooks.terminate_process) {
    m_hooks.terminate_process();
  }
  resetToStopped();
  m_last_error.clear();
  return true;
}

void PlaySessionController::poll() {
  if (m_state == PlaySessionState::Stopped) {
    return;
  }

  if (m_hooks.is_process_running && !m_hooks.is_process_running()) {
    onProcessGone();
    return;
  }

  if (m_state != PlaySessionState::Starting) {
    return;
  }

  if (!m_ipc_connected) {
    if (!m_hooks.ipc_connect || !m_hooks.ipc_connect(m_endpoint)) {
      return;
    }
    m_ipc_connected = true;
  }

  if (!m_ready) {
    if (!m_hooks.ipc_wait_ready || !m_hooks.ipc_wait_ready(0)) {
      return;
    }
    m_ready = true;
    m_state = PlaySessionState::Playing;
  }
}

}  // namespace Blunder
