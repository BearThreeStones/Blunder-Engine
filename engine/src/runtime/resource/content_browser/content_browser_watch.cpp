#include "runtime/resource/content_browser/content_browser_watch.h"

#include <cstring>

#include "runtime/core/base/macro.h"

namespace Blunder {

namespace {

bool pathContainsToken(const std::string& path, const char* token) {
  return path.find(token) != std::string::npos;
}

}  // namespace

void ContentBrowserWatch::initialize(FileSystem* file_system) {
  m_file_system = file_system;
}

void ContentBrowserWatch::shutdown() { stop(); }

void ContentBrowserWatch::start() {
#if defined(BLUNDER_HAS_EFSW)
  if (!m_file_system || m_started.exchange(true)) {
    return;
  }

  const std::string assets_path = m_file_system->getAssetRoot().generic_string();
  const std::string resources_path =
      m_file_system->getResourcesRoot().generic_string();

  m_assets_watch_id =
      m_watcher.addWatch(assets_path, this, true);
  m_resources_watch_id =
      m_watcher.addWatch(resources_path, this, true);

  if (m_assets_watch_id < 0) {
    LOG_WARN("[ContentBrowserWatch] failed to watch Assets: {}",
             assets_path.c_str());
  } else {
    LOG_INFO("[ContentBrowserWatch] watching Assets: {}", assets_path.c_str());
  }

  if (m_resources_watch_id < 0) {
    LOG_WARN("[ContentBrowserWatch] failed to watch Resources: {}",
             resources_path.c_str());
  } else {
    LOG_INFO("[ContentBrowserWatch] watching Resources: {}",
             resources_path.c_str());
  }

  m_watcher.watch();
#else
  LOG_INFO("[ContentBrowserWatch] efsw not available; file watch disabled");
#endif
}

void ContentBrowserWatch::stop() {
#if defined(BLUNDER_HAS_EFSW)
  if (!m_started.exchange(false)) {
    return;
  }

  if (m_assets_watch_id >= 0) {
    m_watcher.removeWatch(m_assets_watch_id);
    m_assets_watch_id = 0;
  }
  if (m_resources_watch_id >= 0) {
    m_watcher.removeWatch(m_resources_watch_id);
    m_resources_watch_id = 0;
  }
#endif
}

void ContentBrowserWatch::suppressNotificationsFor(
    std::chrono::milliseconds duration) {
  const auto until = std::chrono::steady_clock::now() + duration;
  std::lock_guard<std::mutex> lock(m_timing_mutex);
  m_suppress_until = until;
}

bool ContentBrowserWatch::consumeRefreshRequest() {
  if (!m_dirty.load()) {
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(m_timing_mutex);
    if (now < m_suppress_until) {
      return false;
    }
    if (now - m_last_event_time < k_debounce_delay) {
      return false;
    }
  }

  m_dirty.store(false);
  return true;
}

#if defined(BLUNDER_HAS_EFSW)

bool ContentBrowserWatch::shouldIgnoreEvent(const std::string& dir,
                                            const std::string& filename,
                                            const std::string& old_filename) const {
  if (pathContainsToken(dir, ".blunder") ||
      pathContainsToken(filename, ".blunder") ||
      pathContainsToken(old_filename, ".blunder")) {
    return true;
  }

  if (filename == ".gitkeep" || old_filename == ".gitkeep") {
    return true;
  }

  return false;
}

void ContentBrowserWatch::markDirty() {
  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(m_timing_mutex);
    if (now < m_suppress_until) {
      return;
    }
    m_last_event_time = now;
  }
  m_dirty.store(true);
}

void ContentBrowserWatch::handleFileAction(efsw::WatchID watch_id,
                                           const std::string& dir,
                                           const std::string& filename,
                                           efsw::Action action,
                                           const std::string& old_filename) {
  (void)watch_id;
  (void)action;
  if (shouldIgnoreEvent(dir, filename, old_filename)) {
    return;
  }
  markDirty();
}

#endif

}  // namespace Blunder
