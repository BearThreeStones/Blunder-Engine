#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "runtime/platform/file_system/file_system.h"

#if defined(BLUNDER_HAS_EFSW)
#include <efsw/efsw.hpp>
#endif

namespace Blunder {

class AssetCompilerService;
class AssetRegistry;

/// Watches Assets/ (Content Browser refresh) and Intermediate Resources/
/// (Final invalidation). Excludes Resources/Source from Intermediate invalidate.
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

  /// Optional: when set, Intermediate/descriptor changes call
  /// rebuildDependencyGraph + invalidateAssetAndDependents after debounce.
  void setInvalidateTargets(AssetCompilerService* asset_compiler,
                            AssetRegistry* asset_registry);

  /// Call from the main thread each frame; returns true once when a refresh should run.
  bool consumeRefreshRequest();

  /// Debounced Intermediate/descriptor invalidation via AssetCompilerService.
  /// No-op when compiler/registry are unset. Returns true when invalidation ran.
  bool consumeInvalidateRequest();

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
  void markRefreshDirty();
  void queueInvalidatePath(const std::string& absolute_path);

  efsw::FileWatcher m_watcher;
  efsw::WatchID m_assets_watch_id{0};
  efsw::WatchID m_resources_watch_id{0};
#endif

  FileSystem* m_file_system{nullptr};
  AssetCompilerService* m_asset_compiler{nullptr};
  AssetRegistry* m_asset_registry{nullptr};

  std::atomic<bool> m_dirty{false};
  std::atomic<bool> m_invalidate_dirty{false};
  std::atomic<bool> m_started{false};
  std::mutex m_timing_mutex;
  std::chrono::steady_clock::time_point m_last_event_time{};
  std::chrono::steady_clock::time_point m_last_invalidate_event_time{};
  std::chrono::steady_clock::time_point m_suppress_until{};
  std::mutex m_pending_mutex;
  std::vector<std::string> m_pending_invalidate_paths;
  static constexpr std::chrono::milliseconds k_debounce_delay{300};
};

}  // namespace Blunder
