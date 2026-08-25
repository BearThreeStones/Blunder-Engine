#include "runtime/core/log/log_system.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <cstdio>
#include <windows.h>
#endif

namespace Blunder {
namespace {

#ifdef _WIN32
// WIN32_EXECUTABLE editor has no console. Allocate one so stdout_color_sink_mt
// / cerr still show (do not AttachConsole: VS F5 parent has no visible window).
void ensureWin32StdioConsole() {
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

LogSystem::LogSystem() {
#ifdef _WIN32
  ensureWin32StdioConsole();
#endif
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::trace);
  console_sink->set_pattern("[%^%l%$] %v");

  const spdlog::sinks_init_list sink_list = {console_sink};

  spdlog::init_thread_pool(8192, 1);

  m_logger = std::make_shared<spdlog::async_logger>(
      "muggle_logger", sink_list.begin(), sink_list.end(),
      spdlog::thread_pool(), spdlog::async_overflow_policy::block);
  m_logger->set_level(spdlog::level::trace);

  spdlog::register_logger(m_logger);
}

LogSystem::~LogSystem() {
  m_logger->flush();
  spdlog::drop_all();
}

}  // namespace Blunder