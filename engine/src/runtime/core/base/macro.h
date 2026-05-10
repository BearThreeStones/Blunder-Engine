#pragma once

#include <chrono>
#include <thread>

#include <cassert>

#include "runtime/core/log/log_access.h"
#include "runtime/core/log/log_system.h"

// Log macros compatible with fmt v12 (requires compile-time format strings)
// Usage: LOG_DEBUG("message with {} args", arg1);
#define LOG_DEBUG(fmt, ...) \
  Blunder::getLogSystem()->log( \
      Blunder::LogSystem::LogLevel::debug, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_INFO(fmt, ...) \
  Blunder::getLogSystem()->log( \
      Blunder::LogSystem::LogLevel::info, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_WARN(fmt, ...) \
  Blunder::getLogSystem()->log( \
      Blunder::LogSystem::LogLevel::warn, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
  Blunder::getLogSystem()->log( \
      Blunder::LogSystem::LogLevel::error, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
  Blunder::getLogSystem()->log( \
      Blunder::LogSystem::LogLevel::fatal, fmt __VA_OPT__(,) __VA_ARGS__)

#define PolitSleep(_ms) \
  std::this_thread::sleep_for(std::chrono::milliseconds(_ms));

#define PolitNameOf(name) #name

#ifdef NDEBUG
#define ASSERT(statement)
#else
#define ASSERT(statement) assert(statement)
#endif