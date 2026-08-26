#include "runtime/core/log/console_ring.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <unordered_map>

namespace Blunder {
namespace {

int64_t nowUnixMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

ConsoleRing& ConsoleRing::instance() {
  static ConsoleRing ring;
  return ring;
}

void ConsoleRing::setForwardEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_forward_enabled = enabled;
}

bool ConsoleRing::forwardEnabled() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_forward_enabled;
}

void ConsoleRing::append(ConsoleSeverity severity, ConsoleOrigin origin,
                         std::string text, std::string stack) {
  ConsoleMessage message;
  message.severity = severity;
  message.origin = origin;
  message.text = std::move(text);
  message.stack = std::move(stack);
  message.unix_ms = nowUnixMs();
  append(std::move(message));
}

void ConsoleRing::append(ConsoleMessage message) {
  std::lock_guard<std::mutex> lock(m_mutex);
  message.text = truncateConsoleField(std::move(message.text), kConsoleIpcFieldCap);
  message.stack =
      truncateConsoleField(std::move(message.stack), kConsoleIpcFieldCap);
  if (message.unix_ms == 0) {
    message.unix_ms = nowUnixMs();
  }
  if (m_forward_enabled) {
    m_pending_forward.push_back(message);
    while (m_pending_forward.size() > kConsoleCapacity) {
      m_pending_forward.erase(m_pending_forward.begin());
    }
  }
  m_messages.push_back(std::move(message));
  while (m_messages.size() > kConsoleCapacity) {
    m_messages.erase(m_messages.begin());
  }
  ++m_generation;
}

void ConsoleRing::clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_messages.clear();
  m_pending_forward.clear();
  ++m_generation;
}

size_t ConsoleRing::size() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_messages.size();
}

std::vector<ConsoleMessage> ConsoleRing::snapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_messages;
}

void ConsoleRing::severityCounts(size_t& out_log, size_t& out_warning,
                                 size_t& out_error) const {
  out_log = 0;
  out_warning = 0;
  out_error = 0;
  std::lock_guard<std::mutex> lock(m_mutex);
  for (const ConsoleMessage& msg : m_messages) {
    switch (msg.severity) {
      case ConsoleSeverity::Log:
        ++out_log;
        break;
      case ConsoleSeverity::Warning:
        ++out_warning;
        break;
      case ConsoleSeverity::Error:
        ++out_error;
        break;
    }
  }
}

uint64_t ConsoleRing::generation() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_generation;
}

std::vector<ConsoleMessage> ConsoleRing::takePendingForward() {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::vector<ConsoleMessage> out;
  out.swap(m_pending_forward);
  return out;
}

std::string formatConsoleTime(int64_t unix_ms) {
  if (unix_ms < 0) {
    return "00:00:00";
  }
  const std::time_t seconds = static_cast<std::time_t>(unix_ms / 1000);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &seconds) != 0) {
    return "00:00:00";
  }
#else
  if (localtime_r(&seconds, &local) == nullptr) {
    return "00:00:00";
  }
#endif
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", local.tm_hour, local.tm_min,
                local.tm_sec);
  return std::string(buf);
}

std::string truncateConsoleField(std::string value, size_t max_bytes) {
  if (value.size() <= max_bytes) {
    return value;
  }
  value.resize(max_bytes);
  return value;
}

ConsoleViewSettings& consoleViewSettings() {
  static ConsoleViewSettings settings;
  return settings;
}

namespace {

std::string asciiLower(std::string value) {
  for (char& c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

bool consoleSeverityVisible(ConsoleSeverity severity,
                            const ConsoleViewSettings& settings) {
  switch (severity) {
    case ConsoleSeverity::Log:
      return settings.show_log;
    case ConsoleSeverity::Warning:
      return settings.show_warning;
    case ConsoleSeverity::Error:
      return settings.show_error;
  }
  return false;
}

bool consoleMessageMatchesSearch(const ConsoleMessage& message,
                                 const std::string& needle_lower) {
  if (needle_lower.empty()) {
    return true;
  }
  if (asciiLower(message.text).find(needle_lower) != std::string::npos) {
    return true;
  }
  return asciiLower(message.stack).find(needle_lower) != std::string::npos;
}

std::string consoleCollapseKey(const ConsoleMessage& message) {
  std::string key;
  key.push_back(static_cast<char>(static_cast<uint8_t>(message.severity)));
  key.push_back(static_cast<char>(static_cast<uint8_t>(message.origin)));
  key += message.text;
  key.push_back('\0');
  key += message.stack;
  return key;
}

}  // namespace

std::vector<ConsoleVisibleRow> buildConsoleVisibleRows(
    const std::vector<ConsoleMessage>& messages,
    const ConsoleViewSettings& settings) {
  const std::string needle = asciiLower(settings.search);
  std::vector<ConsoleMessage> filtered;
  filtered.reserve(messages.size());
  for (const ConsoleMessage& message : messages) {
    if (!consoleSeverityVisible(message.severity, settings)) {
      continue;
    }
    if (!consoleMessageMatchesSearch(message, needle)) {
      continue;
    }
    filtered.push_back(message);
  }

  if (!settings.collapse) {
    std::vector<ConsoleVisibleRow> rows;
    rows.reserve(filtered.size());
    for (ConsoleMessage& message : filtered) {
      ConsoleVisibleRow row;
      row.message = std::move(message);
      row.count = 1;
      rows.push_back(std::move(row));
    }
    return rows;
  }

  std::vector<ConsoleVisibleRow> rows;
  std::unordered_map<std::string, size_t> index_by_key;
  for (ConsoleMessage& message : filtered) {
    const std::string key = consoleCollapseKey(message);
    const auto it = index_by_key.find(key);
    if (it == index_by_key.end()) {
      index_by_key.emplace(key, rows.size());
      ConsoleVisibleRow row;
      row.message = std::move(message);
      row.count = 1;
      rows.push_back(std::move(row));
      continue;
    }
    ConsoleVisibleRow& row = rows[it->second];
    row.message = std::move(message);
    ++row.count;
  }
  return rows;
}

}  // namespace Blunder
