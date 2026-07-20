#pragma once

#include "EASTL/string.h"

#include <filesystem>

namespace Blunder {

struct ScriptsBuildResult {
  bool ok{false};
  eastl::string error;
  std::filesystem::path output_dll;
};

/// Invokes `dotnet build` on the Project `Scripts/*.csproj` and places
/// output under `<project>/.blunder/scripts_bin`. Passes
/// `-p:BlunderEngineDir=` pointing at the directory beside the running
/// executable (where `Blunder.Api.dll` is staged).
ScriptsBuildResult buildProjectScripts(
    const std::filesystem::path& project_root);

}  // namespace Blunder
