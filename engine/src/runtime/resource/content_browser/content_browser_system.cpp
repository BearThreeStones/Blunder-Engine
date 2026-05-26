#include "runtime/resource/content_browser/content_browser_system.h"

#include <cstring>

#include <filesystem>

#include "runtime/core/base/macro.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/content/content_index.h"
#include "runtime/resource/thumbnail/thumbnail_generator.h"
#include "runtime/function/global/global_context.h"

namespace Blunder {

namespace {

namespace fs = std::filesystem;

bool endsWith(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
}

eastl::string parentVirtualPath(const eastl::string& virtual_path) {
  const size_t slash = virtual_path.find_last_of('/');
  if (slash == eastl::string::npos || slash == 0) {
    return eastl::string();
  }
  return virtual_path.substr(0, slash + 1);
}

}  // namespace

void ContentBrowserSystem::initialize(const ContentBrowserInit& init) {
  m_file_system = init.file_system;
  m_asset_manager = init.asset_manager;
  m_thumbnail_generator = init.thumbnail_generator;
  m_active_root = ContentRoot::Assets;
  m_selected_folder = rootVirtualPath(m_active_root);
  m_is_initialized = m_file_system != nullptr && m_thumbnail_generator != nullptr;
  m_file_watch.initialize(m_file_system);
}

void ContentBrowserSystem::shutdown() {
  stopFileWatch();
  m_file_watch.shutdown();
  m_entries.clear();
  m_entry_index.clear();
  m_expanded_folders.clear();
  m_tree_rows.clear();
  m_grid_items.clear();
  m_drag.reset();
  m_file_system = nullptr;
  m_asset_manager = nullptr;
  m_thumbnail_generator = nullptr;
  m_is_initialized = false;
}

eastl::string ContentBrowserSystem::rootVirtualPath(ContentRoot root) {
  return root == ContentRoot::Assets ? eastl::string("assets/")
                                     : eastl::string("resources/");
}

ContentRoot ContentBrowserSystem::rootFromVirtualPath(
    const eastl::string& virtual_path) {
  if (virtual_path.compare(0, 7, "assets/") == 0 ||
      virtual_path == "assets") {
    return ContentRoot::Assets;
  }
  return ContentRoot::Resources;
}

eastl::string ContentBrowserSystem::displayNameFromPath(
    const eastl::string& virtual_path) {
  if (virtual_path.empty()) {
    return eastl::string();
  }
  size_t end = virtual_path.size();
  if (virtual_path.back() == '/') {
    end -= 1;
  }
  const size_t slash = virtual_path.find_last_of('/', end - 1);
  if (slash == eastl::string::npos) {
    return virtual_path.substr(0, end);
  }
  return virtual_path.substr(slash + 1, end - slash - 1);
}

bool ContentBrowserSystem::isDescendantPath(const eastl::string& ancestor,
                                            const eastl::string& path) {
  if (ancestor.empty() || path.size() <= ancestor.size()) {
    return false;
  }
  if (path.compare(0, ancestor.size(), ancestor) != 0) {
    return false;
  }
  return path[ancestor.size()] == '/';
}

eastl::string ContentBrowserSystem::joinVirtualPath(
    const eastl::string& folder, const eastl::string& file_name) {
  eastl::string result = folder;
  if (!result.empty() && result.back() != '/') {
    result.push_back('/');
  }
  result.append(file_name);
  return result;
}

std::filesystem::path ContentBrowserSystem::resolveVirtualAbsolute(
    const eastl::string& virtual_path) const {
  ASSERT(m_file_system);
  const ContentRoot root = rootFromVirtualPath(virtual_path);
  eastl::string relative = virtual_path;
  if (root == ContentRoot::Assets) {
    if (relative.compare(0, 7, "assets/") == 0) {
      relative.erase(0, 7);
    }
    return m_file_system->resolveAsset(
        std::filesystem::path(relative.c_str()));
  }
  if (relative.compare(0, 10, "resources/") == 0) {
    relative.erase(0, 10);
  }
  return m_file_system->resolveResource(
      std::filesystem::path(relative.c_str()));
}

eastl::string ContentBrowserSystem::makeUniqueDestinationName(
    const eastl::string& folder_virtual, const eastl::string& file_name) const {
  eastl::string candidate = joinVirtualPath(folder_virtual, file_name);
  const fs::path absolute = resolveVirtualAbsolute(candidate);
  if (!findEntry(candidate) && !m_file_system->exists(absolute)) {
    return file_name;
  }

  const size_t dot = file_name.find_last_of('.');
  eastl::string stem =
      dot == eastl::string::npos ? file_name : file_name.substr(0, dot);
  eastl::string ext =
      dot == eastl::string::npos ? eastl::string() : file_name.substr(dot);

  for (uint32_t index = 1; index < 10000; ++index) {
    char suffix[16];
    std::snprintf(suffix, sizeof(suffix), "_%u", index);
    eastl::string alt_name = stem;
    alt_name.append(suffix);
    alt_name.append(ext);
    candidate = joinVirtualPath(folder_virtual, alt_name);
    if (!findEntry(candidate)) {
      const fs::path absolute = resolveVirtualAbsolute(candidate);
      if (!m_file_system->exists(absolute)) {
        return alt_name;
      }
    }
  }
  return file_name;
}

void ContentBrowserSystem::setActiveRoot(ContentRoot root) {
  m_active_root = root;
  m_selected_folder = rootVirtualPath(root);
  m_expanded_folders.clear();
  m_expanded_folders[m_selected_folder] = true;
  rebuildVisibleTree();
  rebuildGrid();
}

void ContentBrowserSystem::setSelectedFolder(const eastl::string& virtual_path) {
  if (virtual_path.empty()) {
    return;
  }
  m_selected_folder = virtual_path;
  if (!endsWith(m_selected_folder, "/")) {
    m_selected_folder.push_back('/');
  }
  m_expanded_folders[m_selected_folder] = true;
  rebuildVisibleTree();
  rebuildGrid();
}

void ContentBrowserSystem::toggleFolderExpanded(
    const eastl::string& virtual_path) {
  eastl::string folder = virtual_path;
  if (!folder.empty() && !endsWith(folder, "/")) {
    folder.push_back('/');
  }
  const bool expanded_before = isFolderExpanded(folder);
  m_expanded_folders[folder] = !expanded_before;
  rebuildVisibleTree();
}

bool ContentBrowserSystem::isFolderExpanded(
    const eastl::string& virtual_path) const {
  const auto it = m_expanded_folders.find(virtual_path);
  if (it != m_expanded_folders.end()) {
    return it->second;
  }
  return virtual_path == rootVirtualPath(m_active_root);
}

void ContentBrowserSystem::indexEntries() {
  m_entry_index.clear();
  for (size_t index = 0; index < m_entries.size(); ++index) {
    m_entry_index[m_entries[index].virtual_path] = index;
  }
}

const ContentEntry* ContentBrowserSystem::findEntry(
    const eastl::string& virtual_path) const {
  const auto it = m_entry_index.find(virtual_path);
  if (it == m_entry_index.end()) {
    return nullptr;
  }
  return &m_entries[it->second];
}

void ContentBrowserSystem::startFileWatch() {
  if (!m_is_initialized) {
    return;
  }
  m_file_watch.start();
}

void ContentBrowserSystem::stopFileWatch() { m_file_watch.stop(); }

bool ContentBrowserSystem::tickFileWatch() {
  return m_file_watch.consumeRefreshRequest();
}

ContentBrowserRefreshStats ContentBrowserSystem::refresh() {
  ContentBrowserRefreshStats stats{};
  if (!m_is_initialized) {
    return stats;
  }

  m_file_watch.suppressNotificationsFor(std::chrono::milliseconds(750));

  m_entries = buildContentIndexWithThumbnails(
      *m_file_system, *m_asset_manager, *m_thumbnail_generator);
  stats.entry_count = static_cast<uint32_t>(m_entries.size());
  for (const ContentEntry& entry : m_entries) {
    switch (entry.thumbnail_status) {
      case ThumbnailStatus::Generated:
        ++stats.thumbnails_generated;
        break;
      case ThumbnailStatus::CacheHit:
        ++stats.thumbnails_cached;
        break;
      case ThumbnailStatus::Skipped:
        ++stats.thumbnails_skipped;
        break;
      case ThumbnailStatus::Failed:
        ++stats.thumbnails_failed;
        break;
      default:
        break;
    }
  }

  indexEntries();
  if (m_selected_folder.empty()) {
    m_selected_folder = rootVirtualPath(m_active_root);
  }
  m_expanded_folders[m_selected_folder] = true;
  rebuildVisibleTree();
  rebuildGrid();
  return stats;
}

void ContentBrowserSystem::rebuildGrid() {
  m_grid_items.clear();
  if (m_selected_folder.empty()) {
    return;
  }

  for (const ContentEntry& entry : m_entries) {
    if (entry.is_directory) {
      continue;
    }
    if (rootFromVirtualPath(entry.virtual_path) != m_active_root) {
      continue;
    }
    const eastl::string parent = parentVirtualPath(entry.virtual_path);
  eastl::string normalized_parent = parent;
    if (!normalized_parent.empty() && !endsWith(normalized_parent, "/")) {
      normalized_parent.push_back('/');
    }
    if (normalized_parent != m_selected_folder) {
      continue;
    }

    ContentBrowserGridItem item{};
    item.virtual_path = entry.virtual_path;
    item.display_name = displayNameFromPath(entry.virtual_path);
    item.thumbnail_cache_path = entry.thumbnail_cache_path;
    item.thumbnail_status = entry.thumbnail_status;
    m_grid_items.push_back(item);
  }

  std::sort(m_grid_items.begin(), m_grid_items.end(),
            [](const ContentBrowserGridItem& a, const ContentBrowserGridItem& b) {
              return a.display_name < b.display_name;
            });
}

void ContentBrowserSystem::rebuildVisibleTree() {
  m_tree_rows.clear();
  const eastl::string root_path = rootVirtualPath(m_active_root);

  eastl::vector<const ContentEntry*> directories;
  for (const ContentEntry& entry : m_entries) {
    if (!entry.is_directory) {
      continue;
    }
    if (rootFromVirtualPath(entry.virtual_path) != m_active_root) {
      continue;
    }
    directories.push_back(&entry);
  }

  std::sort(directories.begin(), directories.end(),
            [](const ContentEntry* a, const ContentEntry* b) {
              return a->virtual_path < b->virtual_path;
            });

  auto appendFolder = [&](const eastl::string& folder_path, int32_t depth) {
    const ContentEntry* entry = findEntry(folder_path);
    if (entry == nullptr) {
      return;
    }

    bool has_children = false;
    for (const ContentEntry* other : directories) {
      if (other->virtual_path == folder_path) {
        continue;
      }
      if (isDescendantPath(folder_path, other->virtual_path)) {
        const eastl::string parent = parentVirtualPath(other->virtual_path);
        eastl::string normalized_parent = parent;
        if (!normalized_parent.empty() && !endsWith(normalized_parent, "/")) {
          normalized_parent.push_back('/');
        }
        if (normalized_parent == folder_path) {
          has_children = true;
          break;
        }
      }
    }

    ContentBrowserTreeRow row{};
    row.virtual_path = folder_path;
    row.display_name = displayNameFromPath(folder_path);
    row.depth = depth;
    row.is_directory = true;
    row.has_children = has_children;
    row.is_expanded = isFolderExpanded(folder_path);
    m_tree_rows.push_back(row);
  };

  auto walk = [&](auto&& self, const eastl::string& folder_path,
                  int32_t depth) -> void {
    appendFolder(folder_path, depth);
    if (!isFolderExpanded(folder_path)) {
      return;
    }
    for (const ContentEntry* other : directories) {
      eastl::string parent = parentVirtualPath(other->virtual_path);
      if (!parent.empty() && !endsWith(parent, "/")) {
        parent.push_back('/');
      }
      if (parent == folder_path && other->virtual_path != folder_path) {
        self(self, other->virtual_path, depth + 1);
      }
    }
  };

  walk(walk, root_path, 0);
}

bool ContentBrowserSystem::reparentEntry(
    const eastl::string& source_virtual_path,
    const eastl::string& target_folder_virtual_path) {
  if (!m_is_initialized || source_virtual_path.empty() ||
      target_folder_virtual_path.empty()) {
    return false;
  }

  const ContentEntry* source = findEntry(source_virtual_path);
  const ContentEntry* target = findEntry(target_folder_virtual_path);
  if (source == nullptr || target == nullptr || source->is_directory ||
      !target->is_directory) {
    LOG_WARN("[ContentBrowser] reparent rejected: invalid source/target");
    return false;
  }

  if (source->root != target->root) {
    LOG_WARN("[ContentBrowser] reparent rejected: cross-root move");
    return false;
  }

  if (isDescendantPath(source_virtual_path, target_folder_virtual_path)) {
    LOG_WARN("[ContentBrowser] reparent rejected: target inside source");
    return false;
  }

  const eastl::string file_name = displayNameFromPath(source_virtual_path);
  const eastl::string dest_virtual =
      joinVirtualPath(target_folder_virtual_path, file_name);
  if (dest_virtual == source_virtual_path) {
    return true;
  }

  const fs::path src_abs = resolveVirtualAbsolute(source_virtual_path);
  const fs::path dst_abs = resolveVirtualAbsolute(dest_virtual);
  if (!m_file_system->movePath(src_abs, dst_abs)) {
    LOG_ERROR("[ContentBrowser] reparent move failed {} -> {}",
              source_virtual_path.c_str(), dest_virtual.c_str());
    return false;
  }

  LOG_INFO("[ContentBrowser] moved {} -> {}", source_virtual_path.c_str(),
           dest_virtual.c_str());
  refresh();
  return true;
}

uint32_t ContentBrowserSystem::importExternalFiles(
    const eastl::vector<eastl::string>& absolute_paths,
    const eastl::string& target_folder_virtual_path) {
  if (!m_is_initialized || target_folder_virtual_path.empty()) {
    return 0;
  }

  const ContentEntry* target = findEntry(target_folder_virtual_path);
  if (target == nullptr || !target->is_directory) {
    LOG_WARN("[ContentBrowser] import rejected: invalid target folder {}",
             target_folder_virtual_path.c_str());
    return 0;
  }

  uint32_t imported = 0;
  for (const eastl::string& absolute_path : absolute_paths) {
    const fs::path src(absolute_path.c_str());
    std::error_code ec;
    if (!fs::is_regular_file(src, ec)) {
      continue;
    }

    const eastl::string file_name(src.filename().generic_string().c_str());
    const eastl::string unique_name =
        makeUniqueDestinationName(target_folder_virtual_path, file_name);
    const eastl::string dest_virtual =
        joinVirtualPath(target_folder_virtual_path, unique_name);
    const fs::path dst = resolveVirtualAbsolute(dest_virtual);

    if (m_file_system->copyFile(src, dst, false)) {
      ++imported;
      LOG_INFO("[ContentBrowser] imported {} -> {}", absolute_path.c_str(),
               dest_virtual.c_str());
    }
  }

  if (imported > 0) {
    refresh();
  }
  return imported;
}

eastl::string ContentBrowserSystem::hitTestFolderAt(float logical_x,
                                                      float logical_y,
                                                      int32_t tree_origin_y,
                                                      float row_height) const {
  (void)logical_x;
  if (row_height <= 0.0f) {
    return eastl::string();
  }

  const int32_t relative_y =
      static_cast<int32_t>(logical_y) - tree_origin_y;
  if (relative_y < 0) {
    return eastl::string();
  }

  const int32_t row_index = static_cast<int32_t>(relative_y / row_height);
  if (row_index < 0 ||
      row_index >= static_cast<int32_t>(m_tree_rows.size())) {
    return eastl::string();
  }

  const ContentBrowserTreeRow& row = m_tree_rows[static_cast<size_t>(row_index)];
  if (!row.is_directory) {
    return eastl::string();
  }
  return row.virtual_path;
}

}  // namespace Blunder
