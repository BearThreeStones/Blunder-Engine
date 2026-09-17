#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Blunder {

/// NDJSON present/pointer trace. Enabled with BLUNDER_DEBUG_INPUT_PRESENT=1.
/// Optional path: BLUNDER_DEBUG_INPUT_PRESENT_PATH.
inline bool inputPresentTraceEnabled() {
  static int enabled = -1;
  if (enabled < 0) {
    const char* env = std::getenv("BLUNDER_DEBUG_INPUT_PRESENT");
    enabled = (env != nullptr && env[0] != '\0' && env[0] != '0') ? 1 : 0;
  }
  return enabled == 1;
}

inline void traceInputPresent(const char* kind, const char* data_json) {
  if (!inputPresentTraceEnabled() || kind == nullptr) {
    return;
  }
  static FILE* file = nullptr;
  static bool opened = false;
  if (!opened) {
    opened = true;
    const char* path = std::getenv("BLUNDER_DEBUG_INPUT_PRESENT_PATH");
    if (path == nullptr || path[0] == '\0') {
      path = "input-present-trace.jsonl";
    }
    file = std::fopen(path, "ab");
  }
  if (file == nullptr) {
    return;
  }
  const auto now_ms =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  std::fprintf(file, "{\"tMs\":%.3f,\"kind\":\"%s\",\"data\":%s}\n", now_ms, kind,
               data_json != nullptr ? data_json : "{}");
  std::fflush(file);
}

/// Per-phase wall-clock split of one function body. `mark()` records the time
/// since the previous mark; the destructor emits a single record so every
/// early-return path is covered.
class InputPresentPhaseTrace {
 public:
  explicit InputPresentPhaseTrace(const char* kind)
      : m_kind(kind),
        m_enabled(inputPresentTraceEnabled()),
        m_t0(std::chrono::steady_clock::now()),
        m_last(m_t0) {
    m_buf[0] = '{';
    m_buf[1] = '\0';
    m_len = 1;
  }

  ~InputPresentPhaseTrace() {
    if (!m_enabled) {
      return;
    }
    const double total_ms = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - m_t0)
                                .count();
    append(",\"totalMs\":%.2f}", total_ms);
    traceInputPresent(m_kind, m_buf);
  }

  void mark(const char* name) {
    if (!m_enabled) {
      return;
    }
    const auto now = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(now - m_last).count();
    m_last = now;
    append("%s\"%s\":%.2f", m_len > 1 ? "," : "", name, ms);
  }

  void flag(const char* name, int value) {
    if (!m_enabled) {
      return;
    }
    append("%s\"%s\":%d", m_len > 1 ? "," : "", name, value);
  }

 private:
  template <typename... Args>
  void append(const char* fmt, Args... args) {
    if (m_len >= static_cast<int>(sizeof(m_buf)) - 1) {
      return;
    }
    const int written = std::snprintf(m_buf + m_len, sizeof(m_buf) - m_len, fmt,
                                      args...);
    if (written > 0) {
      m_len += written;
      if (m_len > static_cast<int>(sizeof(m_buf)) - 1) {
        m_len = static_cast<int>(sizeof(m_buf)) - 1;
      }
    }
  }

  const char* m_kind;
  bool m_enabled;
  std::chrono::steady_clock::time_point m_t0;
  std::chrono::steady_clock::time_point m_last;
  char m_buf[768];
  int m_len;
};

}  // namespace Blunder
