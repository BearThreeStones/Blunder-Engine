#pragma once

#include "EASTL/string.h"

#include "runtime/project/project_file.h"

#include <filesystem>

namespace Blunder {

struct CreateProjectRequest {
  eastl::string name;
  std::filesystem::path target_path;
  bool create_folder{true};
};

/// Creates a Project at `target_path` (or a name-based subdirectory when
/// `create_folder` is true). Requires empty or missing target directory.
bool createProject(const CreateProjectRequest& request, ProjectInfo& out_info,
                   eastl::string& out_error);

}  // namespace Blunder
