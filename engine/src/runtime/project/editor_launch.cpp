#include "runtime/project/editor_launch.h"

#include <cstring>

namespace Blunder {

EditorSessionLaunch resolveEditorSessionLaunch(
    int argc, char** argv, bool debug_build,
    const std::filesystem::path& compiled_project_root) {
  EditorSessionLaunch options;
  std::filesystem::path cli_root;

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg == nullptr) {
      continue;
    }
    if (std::strcmp(arg, "--project-root") == 0) {
      if (i + 1 < argc && argv[i + 1] != nullptr) {
        cli_root = argv[++i];
      }
      continue;
    }
  }

  if (!cli_root.empty()) {
    options.ok = true;
    options.project_root = cli_root;
    return options;
  }

  if (debug_build && !compiled_project_root.empty()) {
    options.ok = true;
    options.project_root = compiled_project_root;
    return options;
  }

  options.ok = false;
  options.error =
      "No project root. Launch project_manager.exe, or pass "
      "--project-root <path>.";
  return options;
}

}  // namespace Blunder
