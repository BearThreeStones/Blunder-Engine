#pragma once

#include "runtime/core/log/console_ring.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace Blunder {

class LogSystem final {
 public:
  enum class LogLevel : uint8_t { debug, info, warn, error, fatal };

 public:
  LogSystem();
  ~LogSystem();

  template <typename... TARGS>
  void log(LogLevel level, fmt::format_string<TARGS...> fmt, TARGS&&... args) {
    if (level == LogLevel::debug) {
      m_logger->debug(fmt, std::forward<TARGS>(args)...);
      return;
    }

    const std::string text = fmt::format(fmt, std::forward<TARGS>(args)...);
    if (const auto severity = mapToConsoleSeverity(level)) {
      ConsoleRing::instance().append(*severity, consoleOriginForHost(), text);
    }

    switch (level) {
      case LogLevel::info:
        m_logger->info("{}", text);
        break;
      case LogLevel::warn:
        m_logger->warn("{}", text);
        break;
      case LogLevel::error:
        m_logger->error("{}", text);
        break;
      case LogLevel::fatal:
        m_logger->critical("{}", text);
        throw std::runtime_error(text);
      case LogLevel::debug:
      default:
        break;
    }
  }

 private:
  static std::optional<ConsoleSeverity> mapToConsoleSeverity(LogLevel level) {
    switch (level) {
      case LogLevel::info:
        return ConsoleSeverity::Log;
      case LogLevel::warn:
        return ConsoleSeverity::Warning;
      case LogLevel::error:
      case LogLevel::fatal:
        return ConsoleSeverity::Error;
      case LogLevel::debug:
      default:
        return std::nullopt;
    }
  }

  /// True when an OS console is already attached (no AllocConsole).
  static bool hasAttachedTerminal();

  std::shared_ptr<spdlog::logger> m_logger;
};

}  // namespace Blunder
