#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <filesystem>

namespace Blunder {

/// Args for opening an Editor Session: [exe_placeholder, "--project-root", path].
eastl::vector<eastl::string> buildProjectOpenArgv(
    const std::filesystem::path& project_root);

/// Resolves sibling `engine_editor` next to the current executable
/// (Project Manager and Editor ship in the same output directory).
std::filesystem::path resolveEditorExecutablePath();

/// Spawns `engine_editor` with `--project-root` (does not wait).
bool relaunchEditorWithProject(const std::filesystem::path& project_root);

}  // namespace Blunder
