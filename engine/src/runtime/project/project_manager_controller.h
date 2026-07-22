#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/project/project_create.h"
#include "runtime/project/project_list.h"

#include <filesystem>

namespace Blunder {

/// Domain logic for Project Manager actions (list + create/import/open/remove).
/// UI hosts call this; Open success sets `pendingOpenRoot` for relaunch.
class ProjectManagerController final {
 public:
  explicit ProjectManagerController(std::filesystem::path store_path);

  bool load();
  bool save() const;
  void refreshMissing();

  const eastl::vector<ProjectListEntry>& entries() const {
    return m_list.entries();
  }

  bool createProject(const CreateProjectRequest& request, eastl::string& out_error);
  bool importProject(const std::filesystem::path& path_or_file,
                     eastl::string& out_error);

  /// Opens by list index. Empty string = success (pending open set).
  eastl::string openEntry(size_t index);
  eastl::string openEntryByName(const eastl::string& name);
  bool removeEntry(size_t index);

  const std::filesystem::path& pendingOpenRoot() const {
    return m_pending_open_root;
  }
  void clearPendingOpen() { m_pending_open_root.clear(); }

 private:
  void setPendingOpen(const std::filesystem::path& root);

  ProjectList m_list;
  std::filesystem::path m_store_path;
  std::filesystem::path m_pending_open_root;
};

}  // namespace Blunder
