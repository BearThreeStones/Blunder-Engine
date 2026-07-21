#include "runtime/project/play_preflight.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <system_error>
#include <string>

namespace Blunder {
namespace {

namespace fs = std::filesystem;

fs::file_time_type maxWriteTimeUnder(const fs::path& root,
                                     const std::function<bool(const fs::path&)>&
                                         accept) {
  fs::file_time_type newest = fs::file_time_type::min();
  std::error_code ec;
  if (!fs::exists(root, ec)) {
    return newest;
  }
  for (fs::recursive_directory_iterator it(root, ec), end;
       !ec && it != end; it.increment(ec)) {
    if (ec) {
      break;
    }
    if (!it->is_regular_file(ec) || ec) {
      continue;
    }
    if (!accept(it->path())) {
      continue;
    }
    const auto wt = fs::last_write_time(it->path(), ec);
    if (ec) {
      ec.clear();
      continue;
    }
    newest = std::max(newest, wt);
  }
  return newest;
}

bool isScriptsInput(const fs::path& path) {
  const auto ext = path.extension();
  return ext == ".cs" || ext == ".csproj";
}

bool isScriptsOutputDll(const fs::path& path) {
  if (path.extension() != ".dll") {
    return false;
  }
  const std::string name = path.filename().string();
  return name != "Blunder.Api.dll" && name != "Blunder.ScriptHost.dll";
}

bool hasScriptsCsproj(const fs::path& project_root) {
  const fs::path scripts = project_root / "Scripts";
  std::error_code ec;
  if (!fs::is_directory(scripts, ec)) {
    return false;
  }
  for (const fs::directory_entry& entry : fs::directory_iterator(scripts, ec)) {
    if (ec) {
      break;
    }
    if (entry.is_regular_file(ec) && entry.path().extension() == ".csproj") {
      return true;
    }
  }
  return false;
}

}  // namespace

PlayDirtySceneDecision decidePlayDirtyScene(
    bool scene_dirty, std::optional<PlayDirtySceneChoice> choice) {
  PlayDirtySceneDecision decision;
  if (!scene_dirty) {
    decision.proceed = true;
    return decision;
  }
  if (!choice.has_value()) {
    decision.needs_prompt = true;
    return decision;
  }
  switch (*choice) {
    case PlayDirtySceneChoice::SaveAndPlay:
      decision.proceed = true;
      decision.save_first = true;
      break;
    case PlayDirtySceneChoice::PlayLastSaved:
      decision.proceed = true;
      break;
    case PlayDirtySceneChoice::Cancel:
      break;
  }
  return decision;
}

bool areProjectScriptsDirty(const fs::path& project_root) {
  if (project_root.empty() || !hasScriptsCsproj(project_root)) {
    return false;
  }

  const fs::path scripts = project_root / "Scripts";
  const fs::path out_dir = project_root / ".blunder" / "scripts_bin";

  const auto input_newest = maxWriteTimeUnder(scripts, isScriptsInput);
  if (input_newest == fs::file_time_type::min()) {
    // csproj exists but no inputs scanned - still treat as dirty so Play
    // attempts a build rather than spawning against a missing assembly.
    std::error_code ec;
    const bool has_output =
        maxWriteTimeUnder(out_dir, isScriptsOutputDll) !=
        fs::file_time_type::min();
    return !has_output;
  }

  const auto output_newest = maxWriteTimeUnder(out_dir, isScriptsOutputDll);
  if (output_newest == fs::file_time_type::min()) {
    return true;
  }
  return input_newest > output_newest;
}

PlayScriptsGateResult runPlayScriptsGate(const PlayScriptsGateHooks& hooks) {
  PlayScriptsGateResult result;
  if (!hooks.is_dirty) {
    result.ok = true;
    return result;
  }
  if (!hooks.is_dirty()) {
    result.ok = true;
    return result;
  }
  result.build_invoked = true;
  if (!hooks.build) {
    result.error = "scripts build hook missing";
    return result;
  }
  std::string error;
  if (!hooks.build(error)) {
    result.error = error.empty() ? "scripts build failed" : error;
    return result;
  }
  result.ok = true;
  return result;
}

}  // namespace Blunder
