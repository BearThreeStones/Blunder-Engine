#pragma once

#include "EASTL/string.h"

#include <cstdint>
#include <filesystem>

namespace Blunder {

inline constexpr const char* k_request_launch_project_root_required =
    "launch.project_root_required";
inline constexpr const char* k_request_launch_adapter_conflict =
    "launch.adapter_conflict";
inline constexpr const char* k_request_adapter_windowed_forbidden =
    "adapter.windowed_forbidden";

enum class MachineAdapterKind : uint8_t {
  none = 0,
  mcp,
  cli,
};

struct MachineCliArgs {
  eastl::string verb;
  eastl::string subject;
  eastl::string out_path;
  eastl::string entity;
  eastl::string asset;
  bool save{false};
  uint32_t steps{0};
  float tx{0.0f};
  float ty{0.0f};
  float tz{0.0f};
  float qx{0.0f};
  float qy{0.0f};
  float qz{0.0f};
  float qw{1.0f};
  float sx{1.0f};
  float sy{1.0f};
  float sz{1.0f};
};

struct EditorSessionLaunch {
  bool ok{false};
  std::filesystem::path project_root;
  bool headless{false};
  eastl::string error;
  eastl::string failure_code;
  MachineAdapterKind adapter{MachineAdapterKind::none};
  eastl::string scene;
  MachineCliArgs cli;
};

bool isMachineCliVerb(const char* arg);

/// Resolves the Editor Session project root for `engine_editor`.
/// Prefer `--project-root`; else Debug `compiled_project_root` when allowed
/// and this is not a CLI/MCP adapter launch.
/// `--mcp` or a CLI verb implies Headless. Adapters require `--project-root`.
EditorSessionLaunch resolveEditorSessionLaunch(
    int argc, char** argv, bool debug_build,
    const std::filesystem::path& compiled_project_root);

}  // namespace Blunder
