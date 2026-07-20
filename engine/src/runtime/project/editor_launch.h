#pragma once

#include "EASTL/string.h"

#include <filesystem>

namespace Blunder {

struct EditorSessionLaunch {
  bool ok{false};
  std::filesystem::path project_root;
  eastl::string error;
};

/// Resolves the Editor Session project root for `engine_editor`.
/// Prefer `--project-root`; else Debug `compiled_project_root` when allowed.
/// Project Manager is a separate executable — this never starts Manager UI.
EditorSessionLaunch resolveEditorSessionLaunch(
    int argc, char** argv, bool debug_build,
    const std::filesystem::path& compiled_project_root);

}  // namespace Blunder
