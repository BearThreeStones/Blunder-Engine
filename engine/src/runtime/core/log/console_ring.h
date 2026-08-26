#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Blunder {

enum class ConsoleSeverity : uint8_t {
  Log = 0,
  Warning,
  Error,
};

enum class ConsoleOrigin : uint8_t {
  EditorSession = 0,
  PlayProcess,
};

struct ConsoleMessage {
  ConsoleSeverity severity{ConsoleSeverity::Log};
  ConsoleOrigin origin{ConsoleOrigin::EditorSession};
  std::string text;
  std::string stack;
  /// Unix epoch milliseconds (local wall clock for display formatting).
  int64_t unix_ms{0};
};

/// Session-only Console toolbar state (not persisted).
struct ConsoleViewSettings {
  bool collapse{false};
  bool clear_on_play{true};
  bool error_pause{false};
  bool show_log{true};
  bool show_warning{true};
  bool show_error{true};
  std::string search;
  int selected{-1};
};

struct ConsoleVisibleRow {
  ConsoleMessage message;
  uint32_t count{1};
};

inline constexpr size_t kConsoleCapacity = 10000;
inline constexpr size_t kConsoleIpcFieldCap = 16 * 1024;

/// Mutex-protected Console Message ring (cap 10000, drop oldest).
class ConsoleRing final {
 public:
  static ConsoleRing& instance();

  void append(ConsoleSeverity severity, ConsoleOrigin origin, std::string text,
              std::string stack = {});
  void append(ConsoleMessage message);

  void clear();

  size_t size() const;
  std::vector<ConsoleMessage> snapshot() const;

  /// Pre-collapse counts per severity in the ring.
  void severityCounts(size_t& out_log, size_t& out_warning,
                      size_t& out_error) const;

  /// Generation bumps on every append/clear (UI dirty check).
  uint64_t generation() const;

  /// Player → editor forward queue: take pending emits (clears pending).
  std::vector<ConsoleMessage> takePendingForward();

  /// Whether new appends enqueue for Play IPC forward (Player host).
  void setForwardEnabled(bool enabled);

  bool forwardEnabled() const;

 private:
  ConsoleRing() = default;

  mutable std::mutex m_mutex;
  std::vector<ConsoleMessage> m_messages;
  std::vector<ConsoleMessage> m_pending_forward;
  uint64_t m_generation{0};
  bool m_forward_enabled{false};
};

/// Format Console time as `HH:mm:ss` local from unix_ms.
std::string formatConsoleTime(int64_t unix_ms);

/// Truncate UTF-8 text to at most @p max_bytes (may cut mid-sequence at end).
std::string truncateConsoleField(std::string value, size_t max_bytes);

/// Editor Session vs Play Process based on current host mode.
ConsoleOrigin consoleOriginForHost();

/// Process-wide Console toolbar settings (tests and Play session read this).
ConsoleViewSettings& consoleViewSettings();

/// Filter by severity + case-insensitive search, then optional collapse.
std::vector<ConsoleVisibleRow> buildConsoleVisibleRows(
    const std::vector<ConsoleMessage>& messages,
    const ConsoleViewSettings& settings);

}  // namespace Blunder
