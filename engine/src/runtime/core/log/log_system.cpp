#include "runtime/core/log/log_system.h"

#include <spdlog/async.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <vector>

#ifdef _WIN32
#include <cstdio>
#include <cwchar>
#include <windows.h>
#endif

namespace Blunder {
namespace {

#ifdef _WIN32
bool isCommandLineArgBoundary(wchar_t c) {
  return c == L'\0' || c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ||
         c == L'"';
}

// WIN32_EXECUTABLE editor has no console. Allocate one so stdout_color_sink_mt
// / cerr still show (do not AttachConsole: VS F5 parent has no visible window).
// Skip when `--mcp` (or inherited stdio pipes): AllocConsole + freopen steals
// Cursor's MCP pipes, initialize writes fail with EINVAL, and PeekNamedPipe on
// CONIN$ never sees JSON-RPC.
bool commandLineHasMcpFlag() {
  const wchar_t* cmd = GetCommandLineW();
  if (cmd == nullptr || cmd[0] == L'\0') {
    return false;
  }
  const wchar_t* found = cmd;
  while ((found = wcsstr(found, L"--mcp")) != nullptr) {
    const bool start_ok =
        found == cmd || isCommandLineArgBoundary(found[-1]);
    const bool end_ok = isCommandLineArgBoundary(found[5]);
    if (start_ok && end_ok) {
      return true;
    }
    found += 5;
  }
  return false;
}

bool stdHandleIsPipe(DWORD std_handle) {
  HANDLE handle = GetStdHandle(std_handle);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  return GetFileType(handle) == FILE_TYPE_PIPE;
}

void ensureWin32StdioConsole() {
  if (commandLineHasMcpFlag() || stdHandleIsPipe(STD_INPUT_HANDLE) ||
      stdHandleIsPipe(STD_OUTPUT_HANDLE)) {
    return;
  }

  if (GetConsoleWindow() != nullptr) {
    return;
  }

  HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (std_out != nullptr && std_out != INVALID_HANDLE_VALUE &&
      GetConsoleMode(std_out, &mode)) {
    return;
  }

  if (!AllocConsole()) {
    return;
  }

  FILE* unused = nullptr;
  (void)freopen_s(&unused, "CONOUT$", "w", stdout);
  (void)freopen_s(&unused, "CONOUT$", "w", stderr);
  (void)freopen_s(&unused, "CONIN$", "r", stdin);
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD out_mode = 0;
  if (out != nullptr && out != INVALID_HANDLE_VALUE &&
      GetConsoleMode(out, &out_mode)) {
    out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    (void)SetConsoleMode(out, out_mode);
  }
}
#endif

}  // namespace

bool LogSystem::hasAttachedTerminal() {
#ifdef _WIN32
  if (GetConsoleWindow() != nullptr) {
    return true;
  }
  HANDLE std_out = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if (std_out != nullptr && std_out != INVALID_HANDLE_VALUE &&
      GetConsoleMode(std_out, &mode)) {
    return true;
  }
  return false;
#else
  return true;
#endif
}

LogSystem::LogSystem() {
#ifdef _WIN32
  ensureWin32StdioConsole();
#endif
  std::vector<spdlog::sink_ptr> sinks;
  if (hasAttachedTerminal()) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    console_sink->set_pattern("[%^%l%$] %v");
    sinks.push_back(console_sink);
  }

  spdlog::init_thread_pool(8192, 1);

  if (sinks.empty()) {
    sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
  }
  m_logger = std::make_shared<spdlog::async_logger>(
      "muggle_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(),
      spdlog::async_overflow_policy::block);
  m_logger->set_level(spdlog::level::trace);

  spdlog::register_logger(m_logger);
}

LogSystem::~LogSystem() {
  m_logger->flush();
  spdlog::drop_all();
}

}  // namespace Blunder
