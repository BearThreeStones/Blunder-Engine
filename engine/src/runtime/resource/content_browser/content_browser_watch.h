#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include "runtime/platform/file_system/file_system.h"

#if defined(BLUNDER_HAS_EFSW)
#include <efsw/efsw.hpp>
#endif

namespace Blunder {

/// Watches Assets/ and Resources/ with efsw and requests a debounced content refresh.
class ContentBrowserWatch final
#if defined(BLUNDER_HAS_EFSW)
    : public efsw::FileWatchListener
#endif
{
 public:
  void initialize(FileSystem* file_system);
  void shutdown();
  void start();
  void stop();

  /// Call from the main thread each frame; returns true once when a refresh should run.
  bool consumeRefreshRequest();

  /// Suppress watcher callbacks for a short period (e.g. after self-initiated writes).
  void suppressNotificationsFor(std::chrono::milliseconds duration);

#if defined(BLUNDER_HAS_EFSW)
  void handleFileAction(efsw::WatchID watch_id, const std::string& dir,
                        const std::string& filename, efsw::Action action,
                        const std::string& old_filename) override;
#endif

 private:
#if defined(BLUNDER_HAS_EFSW)
  bool shouldIgnoreEvent(const std::string& dir, const std::string& filename,
                         const std::string& old_filename) const;
  void markDirty();

  efsw::FileWatcher m_watcher;
  efsw::WatchID m_assets_watch_id{0};
  efsw::WatchID m_resources_watch_id{0};
#endif

  FileSystem* m_file_system{nullptr};
  std::atomic<bool> m_dirty{false};
  std::atomic<bool> m_started{false};
  std::mutex m_timing_mutex;
  std::chrono::steady_clock::time_point m_last_event_time{};
  std::chrono::steady_clock::time_point m_suppress_until{};
  static constexpr std::chrono::milliseconds k_debounce_delay{300};
};

}  // namespace Blunder
