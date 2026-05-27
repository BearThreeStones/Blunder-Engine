#pragma once

#include "EASTL/string.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class AssetManager;
class FileSystem;
class SceneInstance;
class SceneSystem;

struct SpawnAssetResult {
  bool success{false};
  EntityId spawned_entity{k_invalid_entity_id};
};

/// Tracks editable scene path, dirty state, and save to .scene.asset.
class EditorSceneEditSystem final {
 public:
  void initialize(FileSystem* file_system, AssetManager* asset_manager,
                  SceneSystem* scene_system);

  void setActiveScenePath(eastl::string virtual_path);
  const eastl::string& activeScenePath() const { return m_active_scene_virtual_path; }

  bool isDirty() const { return m_dirty; }
  void markDirty() { m_dirty = true; }
  void clearDirty() { m_dirty = false; }

  /// Merges childScenes from the loaded SceneAsset when available.
  bool saveActiveScene();

  /// Loads a scene asset, sets it active, and resets editor selection.
  bool openScene(const eastl::string& virtual_path);

  /// Spawns a mesh entity or opens a scene asset at the window position.
  SpawnAssetResult spawnAssetAtWindowPosition(
      const eastl::string& asset_virtual_path, float window_x, float window_y);

 private:
  SpawnAssetResult spawnMeshAsset(const eastl::string& asset_virtual_path,
                                  float window_x, float window_y);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  SceneSystem* m_scene_system{nullptr};
  eastl::string m_active_scene_virtual_path;
  bool m_dirty{false};
};

}  // namespace Blunder
