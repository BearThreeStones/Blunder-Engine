#pragma once

#include "EASTL/string.h"

#include <filesystem>

namespace Blunder {

inline constexpr const char* k_project_file_name = "project.blunder";

struct ProjectInfo {
  eastl::string name;
  std::filesystem::path root;
};

/// True when `root` contains a readable Project File.
bool isProjectDirectory(const std::filesystem::path& root);

/// `path` may be the Project root or the `project.blunder` file itself.
bool readProjectFile(const std::filesystem::path& path, ProjectInfo& out_info);

bool writeProjectFile(const std::filesystem::path& root,
                      const eastl::string& name);

}  // namespace Blunder
