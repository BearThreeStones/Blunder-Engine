#pragma once

#include <cstdint>

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/platform/file_system/file_system.h"
#include "runtime/project/editor_detection_settings.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/content_browser/content_browser_drag.h"
#include "runtime/resource/content_browser/content_browser_types.h"
#include "runtime/resource/content_browser/content_browser_watch.h"
#include "runtime/resource/thumbnail/thumbnail_generation_queue.h"

namespace Blunder {

class AssetCompilerService;
class AssetImportService;
class AssetManager;
class AssetRegistry;
class ThumbnailGenerator;

struct ContentBrowserInit {
  FileSystem* file_system{nullptr};
  AssetManager* asset_manager{nullptr};
  ThumbnailGenerator* thumbnail_generator{nullptr};
  AssetCompilerService* asset_compiler{nullptr};
  AssetRegistry* asset_registry{nullptr};
};

class ContentBrowserSystem final {
 public:
  void initialize(const ContentBrowserInit& init);
  void shutdown();

  /// Starts efsw watch on Assets/ + Resources/ (no-op if efsw unavailable).
  void startFileWatch();
  void stopFileWatch();

  /// Wire AssetImportService for Detection Action Reimport (ADR 0029).
  void setReimportTarget(AssetImportService* asset_import);

  void setDetectionAction(DetectionAction action);
  DetectionAction detectionAction() const;

  /// Prompt-mode Detection: coalesced GUID set awaiting Reimport All / Dismiss.
  bool hasPendingDetectionPrompt() const;
  eastl::vector<eastl::string> pendingDetectionGuids() const;
  bool confirmPendingDetectionReimport();
  void dismissPendingDetectionPrompt();

  /// Debounced refresh / Intermediate invalidation / Detection;
  /// call once per frame on the main thread.
  bool tickFileWatch();

  /// Drain async thumbnail queue; returns true when grid thumbnails changed.
  bool tickThumbnailQueue(uint32_t max_items = 2);

  ContentBrowserRefreshStats refresh();

  void setActiveRoot(ContentRoot root);
  ContentRoot activeRoot() const { return m_active_root; }

  void setSelectedFolder(const eastl::string& virtual_path);
  const eastl::string& selectedFolder() const { return m_selected_folder; }

  const eastl::vector<ContentBrowserTreeRow>& treeRows() const {
    return m_tree_rows;
  }
  const eastl::vector<ContentBrowserGridItem>& gridItems() const {
    return m_grid_items;
  }

  const eastl::vector<ContentBrowserPathSegment>& pathSegments() const {
    return m_path_segments;
  }

  void setSearchFilter(const eastl::string& filter);
  const eastl::string& searchFilter() const { return m_search_filter; }

  void setGridSort(BrowserGridSortColumn column);
  BrowserGridSortColumn gridSortColumn() const { return m_grid_sort_column; }
  bool gridSortAscending() const { return m_grid_sort_ascending; }

  eastl::string statusText() const;

  void toggleFolderExpanded(const eastl::string& virtual_path);
  void rebuildVisibleTree();

  bool reparentEntry(const eastl::string& source_virtual_path,
                     const eastl::string& target_folder_virtual_path);
  bool canReparentEntry(const eastl::string& source_virtual_path,
                        const eastl::string& target_folder_virtual_path) const;

  const eastl::vector<ContentEntry>& indexedEntries() const { return m_entries; }

  ContentBrowserMutateResult createFolder(
      const eastl::string& parent_virtual_path);
  ContentBrowserMutateResult renameEntry(const eastl::string& virtual_path,
                                         const eastl::string& new_name);

  const eastl::string& pendingInlineRenamePath() const {
    return m_pending_inline_rename_path;
  }
  void clearPendingInlineRename() { m_pending_inline_rename_path.clear(); }
  void beginInlineRename(const eastl::string& virtual_path);

  bool removeEmptyFolder(const eastl::string& virtual_path);
  bool recreateFolder(const eastl::string& virtual_path);

  uint32_t importExternalFiles(
      const eastl::vector<eastl::string>& absolute_paths,
      const eastl::string& target_folder_virtual_path);

  ContentBrowserDragController& dragController() { return m_drag; }
  const ContentBrowserDragController& dragController() const { return m_drag; }

  /// Folder row under logical (window) coordinates, or empty if none.
  eastl::string hitTestFolderAt(float logical_x, float logical_y,
                                int32_t tree_origin_y, float row_height) const;

  const ContentEntry* findEntry(const eastl::string& virtual_path) const;

 private:
  static eastl::string rootVirtualPath(ContentRoot root);
  static ContentRoot rootFromVirtualPath(const eastl::string& virtual_path);
  static eastl::string displayNameFromPath(const eastl::string& virtual_path);
  static bool isDescendantPath(const eastl::string& ancestor,
                               const eastl::string& path);
  static eastl::string joinVirtualPath(const eastl::string& folder,
                                       const eastl::string& file_name);

  std::filesystem::path resolveVirtualAbsolute(
      const eastl::string& virtual_path) const;
  eastl::string makeUniqueDestinationName(const eastl::string& folder_virtual,
                                          const eastl::string& file_name) const;
  bool isAssetsRootPath(const eastl::string& virtual_path) const;
  void remapRegistryPrefix(const eastl::string& from_prefix,
                           const eastl::string& to_prefix);
  void retargetSelectedFolder(const eastl::string& from_prefix,
                              const eastl::string& to_prefix);
  ContentBrowserMutateResult failMutate(const char* message) const;
  bool siblingTaken(const eastl::string& parent_folder,
                    const eastl::string& name, bool as_directory) const;

  void rebuildGrid();
  void rebuildPathSegments();
  void indexEntries();
  bool isFolderExpanded(const eastl::string& virtual_path) const;
  void enqueueVisibleGridThumbnails();
  void applyThumbnailCompletion(const ThumbnailQueueCompleted& completed);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  ThumbnailGenerator* m_thumbnail_generator{nullptr};

  ContentRoot m_active_root{ContentRoot::Assets};
  eastl::string m_selected_folder;
  eastl::vector<ContentEntry> m_entries;
  eastl::hash_map<eastl::string, size_t> m_entry_index;
  eastl::hash_map<eastl::string, bool> m_expanded_folders;
  eastl::vector<ContentBrowserTreeRow> m_tree_rows;
  eastl::vector<ContentBrowserGridItem> m_grid_items;
  eastl::vector<ContentBrowserPathSegment> m_path_segments;
  eastl::string m_search_filter;
  BrowserGridSortColumn m_grid_sort_column{BrowserGridSortColumn::name};
  bool m_grid_sort_ascending{true};
  ContentBrowserDragController m_drag;
  ContentBrowserWatch m_file_watch;
  AssetRegistry* m_asset_registry{nullptr};
  eastl::string m_pending_inline_rename_path;
  bool m_is_initialized{false};
};

}  // namespace Blunder
