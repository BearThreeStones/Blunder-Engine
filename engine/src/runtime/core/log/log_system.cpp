#include "runtime/core/log/log_system.h"

#include <spdlog/async.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Blunder {

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
  // Non-Windows: keep stdout sink; product AllocConsole concern is Win32.
  return true;
#endif
}

LogSystem::LogSystem() {
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
