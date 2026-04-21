#pragma once

#include <spdlog/spdlog.h>

#include <cstdint>
#include <stdexcept>

namespace Blunder {

class LogSystem final {
 public:
  enum class LogLevel : uint8_t { debug, info, warn, error, fatal };

 public:
  LogSystem();
  ~LogSystem();

  template <typename... TARGS>
  void log(LogLevel level, fmt::format_string<TARGS...> fmt, TARGS&&... args) {
    switch (level) {
      case LogLevel::debug:
        m_logger->debug(fmt, std::forward<TARGS>(args)...);
        break;
      case LogLevel::info:
        m_logger->info(fmt, std::forward<TARGS>(args)...);
        break;
      case LogLevel::warn:
        m_logger->warn(fmt, std::forward<TARGS>(args)...);
        break;
      case LogLevel::error:
        m_logger->error(fmt, std::forward<TARGS>(args)...);
        break;
      case LogLevel::fatal:
        m_logger->critical(fmt, std::forward<TARGS>(args)...);
        fatalCallback(fmt, std::forward<TARGS>(args)...);
        break;
      default:
        break;
    }
  }

  template <typename... TARGS>
  void fatalCallback(fmt::format_string<TARGS...> fmt, TARGS&&... args) {
    const std::string format_str = fmt::format(fmt, std::forward<TARGS>(args)...);
    throw std::runtime_error(format_str);
  }

 private:
  std::shared_ptr<spdlog::logger> m_logger;
};

}  // namespace Blunder