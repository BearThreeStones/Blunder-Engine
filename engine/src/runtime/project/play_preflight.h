#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace Blunder {

class Scene;

enum class PlayDirtySceneChoice : uint8_t {
  SaveAndPlay = 0,
  PlayLastSaved,
  Cancel,
};

struct PlayDirtySceneDecision {
  bool needs_prompt{false};
  bool proceed{false};
  bool save_first{false};
};

/// Pure decision helper for the dirty-scene Play prompt.
/// When `scene_dirty` is false, proceeds without saving (choice ignored).
/// When dirty and `choice` is nullopt, sets `needs_prompt` and does not proceed.
PlayDirtySceneDecision decidePlayDirtyScene(
    bool scene_dirty, std::optional<PlayDirtySceneChoice> choice = std::nullopt);

/// True when Scripts sources/csproj are newer than `.blunder/scripts_bin`
/// outputs, or when sources exist but no game DLL output is present.
/// Projects without a Scripts folder / csproj are treated as not dirty.
bool areProjectScriptsDirty(const std::filesystem::path& project_root);

struct PlayScriptsGateHooks {
  std::function<bool()> is_dirty;
  std::function<bool(std::string& error)> build;
};

struct PlayScriptsGateResult {
  bool ok{false};
  bool build_invoked{false};
  std::string error;
};

/// Runs the Scripts dirty gate: skip build when clean; build when dirty;
/// failure keeps ok=false.
PlayScriptsGateResult runPlayScriptsGate(const PlayScriptsGateHooks& hooks);

/// True if scene contains at least one entity with a Camera component.
bool sceneAssetHasPlayCamera(const Scene& scene);

struct PlayCameraGateResult {
  bool ok{false};
  std::string error;
};

/// Fails closed when the play entry scene has no Camera component.
PlayCameraGateResult runPlayCameraGate(const Scene& scene);

}  // namespace Blunder
