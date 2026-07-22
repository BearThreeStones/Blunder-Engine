#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <cstdint>
#include <filesystem>

namespace Blunder {

struct ProjectListEntry {
  std::filesystem::path path;
  eastl::string name;
  bool missing{false};
  std::int64_t last_opened_unix{0};
};

class ProjectList final {
 public:
  bool load(const std::filesystem::path& store_path);
  bool save(const std::filesystem::path& store_path) const;

  /// Registers or updates by absolute project root. Reads display name from
  /// Project File when present.
  bool addOrUpdate(const std::filesystem::path& project_root);

  bool remove(const std::filesystem::path& project_root);
  void refreshMissing();

  bool markOpened(const std::filesystem::path& project_root);

  const eastl::vector<ProjectListEntry>& entries() const { return m_entries; }

 private:
  eastl::vector<ProjectListEntry> m_entries;
};

/// Default user-level store path (e.g. %APPDATA%/Blunder/project_list.yaml).
std::filesystem::path defaultProjectListStorePath();

/// Canonical parent directory for user-created Projects (not the engine checkout).
/// Windows: `E:/Blunder Projects`. Ensures the directory exists when possible.
std::filesystem::path defaultProjectsDirectory();

/// Formats Project List last-opened for Manager rows. Unset/non-positive → "—".
eastl::string formatProjectLastOpenedDisplay(std::int64_t last_opened_unix);

}  // namespace Blunder
