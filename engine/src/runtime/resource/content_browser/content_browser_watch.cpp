#include "runtime/resource/content_browser/content_browser_watch.h"

#include <cstring>

#include "runtime/core/base/macro.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_cook/asset_watch_path.h"
#include "runtime/resource/asset_dependency/asset_dependency_graph.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

namespace {

bool pathContainsToken(const std::string& path, const char* token) {
  return path.find(token) != std::string::npos;
}

std::string joinDirFile(const std::string& dir, const std::string& filename) {
  if (dir.empty()) {
    return filename;
  }
  if (filename.empty()) {
    return dir;
  }
  const char last = dir.back();
  if (last == '/' || last == '\\') {
    return dir + filename;
  }
  return dir + "/" + filename;
}

}  // namespace

void ContentBrowserWatch::initialize(FileSystem* file_system) {
  m_file_system = file_system;
}

void ContentBrowserWatch::shutdown() { stop(); }

void ContentBrowserWatch::setInvalidateTargets(
    AssetCompilerService* asset_compiler, AssetRegistry* asset_registry) {
  m_asset_compiler = asset_compiler;
  m_asset_registry = asset_registry;
}

void ContentBrowserWatch::setReimportTarget(AssetImportService* asset_import) {
  m_asset_import = asset_import;
}

void ContentBrowserWatch::start() {
#if defined(BLUNDER_HAS_EFSW)
  if (!m_file_system || m_started.exchange(true)) {
    return;
  }

  const std::string assets_path = m_file_system->getAssetRoot().generic_string();
  const std::string resources_path =
      m_file_system->getResourcesRoot().generic_string();

  m_assets_watch_id = m_watcher.addWatch(assets_path, this, true);
  if (m_assets_watch_id < 0) {
    LOG_WARN("[ContentBrowserWatch] failed to watch Assets: {}",
             assets_path.c_str());
  } else {
    LOG_INFO("[ContentBrowserWatch] watching Assets: {}", assets_path.c_str());
  }

  m_resources_watch_id = m_watcher.addWatch(resources_path, this, true);
  if (m_resources_watch_id < 0) {
    LOG_WARN("[ContentBrowserWatch] failed to watch Resources: {}",
             resources_path.c_str());
  } else {
    LOG_INFO("[ContentBrowserWatch] watching Resources (Intermediate invalidate "
             "+ Source auto-Reimport): {}",
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

  {
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    m_pending_invalidate_paths.clear();
    m_pending_reimport_paths.clear();
  }
  m_dirty.store(false);
  m_invalidate_dirty.store(false);
  m_reimport_dirty.store(false);
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

bool ContentBrowserWatch::consumeInvalidateRequest() {
  if (!m_invalidate_dirty.load()) {
    return false;
  }
  if (!m_asset_compiler || !m_asset_registry || !m_file_system) {
    m_invalidate_dirty.store(false);
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    m_pending_invalidate_paths.clear();
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(m_timing_mutex);
    if (now < m_suppress_until) {
      return false;
    }
    if (now - m_last_invalidate_event_time < k_debounce_delay) {
      return false;
    }
  }

  std::vector<std::string> pending;
  {
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    pending.swap(m_pending_invalidate_paths);
    m_invalidate_dirty.store(false);
  }
  if (pending.empty()) {
    return false;
  }

  m_asset_compiler->rebuildDependencyGraph();
  AssetDependencyGraph graph;
  graph.rebuildFromProject(*m_file_system, *m_asset_registry);

  const auto& assets_root = m_file_system->getAssetRoot();
  const auto& resources_root = m_file_system->getResourcesRoot();

  eastl::vector<eastl::string> guids;
  auto pushUnique = [&guids](const eastl::string& guid) {
    if (guid.empty()) {
      return;
    }
    for (const eastl::string& existing : guids) {
      if (existing == guid) {
        return;
      }
    }
    guids.push_back(guid);
  };

  for (const std::string& path : pending) {
    const AssetWatchPathClass kind =
        classifyAssetWatchPath(path, assets_root, resources_root);
    const eastl::vector<eastl::string> mapped = guidsToInvalidateForWatchedPath(
        kind, path, assets_root, resources_root, *m_asset_registry, graph);
    for (const eastl::string& guid : mapped) {
      pushUnique(guid);
    }
  }

  for (const eastl::string& guid : guids) {
    m_asset_compiler->invalidateAssetAndDependents(guid);
  }

  return !guids.empty();
}

bool ContentBrowserWatch::consumeReimportRequest() {
  if (!m_reimport_dirty.load()) {
    return false;
  }
  if (!m_asset_import || !m_file_system) {
    m_reimport_dirty.store(false);
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    m_pending_reimport_paths.clear();
    return false;
  }

  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(m_timing_mutex);
    if (now < m_suppress_until) {
      return false;
    }
    if (now - m_last_reimport_event_time < k_debounce_delay) {
      return false;
    }
  }

  std::vector<std::string> pending;
  {
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    pending.swap(m_pending_reimport_paths);
    m_reimport_dirty.store(false);
  }
  if (pending.empty()) {
    return false;
  }

  eastl::vector<eastl::string> guids;
  auto pushUnique = [&guids](const eastl::string& guid) {
    if (guid.empty()) {
      return;
    }
    for (const eastl::string& existing : guids) {
      if (existing == guid) {
        return;
      }
    }
    guids.push_back(guid);
  };

  for (const std::string& path : pending) {
    const eastl::vector<eastl::string> mapped =
        m_asset_import->findGuidsByArchivedSource(path);
    for (const eastl::string& guid : mapped) {
      pushUnique(guid);
    }
  }

  bool any = false;
  if (!guids.empty()) {
    any = m_asset_import->requestReimports(guids);
  }
  return any;
}

#if defined(BLUNDER_HAS_EFSW)

bool ContentBrowserWatch::shouldIgnoreEvent(
    const std::string& dir, const std::string& filename,
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

void ContentBrowserWatch::markRefreshDirty() {
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

void ContentBrowserWatch::queueInvalidatePath(const std::string& absolute_path) {
  if (absolute_path.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(m_timing_mutex);
    if (now < m_suppress_until) {
      return;
    }
    m_last_invalidate_event_time = now;
  }
  {
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    m_pending_invalidate_paths.push_back(absolute_path);
  }
  m_invalidate_dirty.store(true);
}

void ContentBrowserWatch::queueReimportPath(const std::string& absolute_path) {
  if (absolute_path.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(m_timing_mutex);
    if (now < m_suppress_until) {
      return;
    }
    m_last_reimport_event_time = now;
  }
  {
    std::lock_guard<std::mutex> lock(m_pending_mutex);
    m_pending_reimport_paths.push_back(absolute_path);
  }
  m_reimport_dirty.store(true);
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
  if (!m_file_system) {
    return;
  }

  const std::string absolute = joinDirFile(dir, filename);
  const AssetWatchPathClass kind = classifyAssetWatchPath(
      absolute, m_file_system->getAssetRoot(),
      m_file_system->getResourcesRoot());

  switch (kind) {
    case AssetWatchPathClass::AssetsTree:
      markRefreshDirty();
      queueInvalidatePath(absolute);
      if (!old_filename.empty()) {
        queueInvalidatePath(joinDirFile(dir, old_filename));
      }
      break;
    case AssetWatchPathClass::IntermediateResource:
      queueInvalidatePath(absolute);
      if (!old_filename.empty()) {
        queueInvalidatePath(joinDirFile(dir, old_filename));
      }
      break;
    case AssetWatchPathClass::SourceArchive:
      queueReimportPath(absolute);
      if (!old_filename.empty()) {
        queueReimportPath(joinDirFile(dir, old_filename));
      }
      break;
    case AssetWatchPathClass::Ignored:
      break;
  }
}

#endif

}  // namespace Blunder
