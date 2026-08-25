#pragma once

#include "EASTL/string.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class AssetManager;
class FileSystem;
class Scene;
class SceneInstance;
class SceneSystem;

struct SpawnAssetResult {
  bool success{false};
  EntityId spawned_entity{k_invalid_entity_id};
};

struct SceneAssetOpResult {
  bool success{false};
  eastl::string path;
};

/// Tracks editable scene path, dirty state, and save to .scene.asset.
class EditorSceneEditSystem final {
 public:
  void initialize(FileSystem* file_system, AssetManager* asset_manager,
                  SceneSystem* scene_system);

  void setActiveScenePath(eastl::string virtual_path);
  /// Updates the open document path without changing dirty or Document History.
  void retargetActiveScenePath(eastl::string virtual_path);
  const eastl::string& activeScenePath() const { return m_active_scene_virtual_path; }

  bool isDirty() const { return m_dirty; }
  void markDirty() { m_dirty = true; }
  void clearDirty() { m_dirty = false; }

  /// Unloads the live document and clears the active path.
  void closeActiveScene();

  /// Writes the active SceneInstance to the active Scene Asset path.
  bool saveActiveScene();

  /// Writes the live document to a new unique Scene Asset path, switches the
  /// active path to it, and keeps Document History (Save As).
  SceneAssetOpResult saveActiveSceneAs();

  /// Creates a starter Scene Asset (default Main Camera) under folder and
  /// returns its virtual path. Caller opens it (with dirty prompt if needed).
  SceneAssetOpResult createNewSceneAsset(const eastl::string& folder_virtual_path);

  /// Copies an on-disk Scene Asset to a unique sibling path with a new GUID.
  SceneAssetOpResult duplicateSceneAsset(const eastl::string& source_virtual_path);

  /// Loads a scene asset, sets it active, and resets editor selection.
  bool openScene(const eastl::string& virtual_path);

  /// Spawns a mesh entity or opens a scene asset at the window position.
  SpawnAssetResult spawnAssetAtWindowPosition(
      const eastl::string& asset_virtual_path, float window_x, float window_y);

  /// Soft-deletes the current selection and records an Editor Command.
  bool softDeleteSelection();

 private:
  SpawnAssetResult spawnMeshAsset(const eastl::string& asset_virtual_path,
                                  float window_x, float window_y);

  bool writeSceneDocument(const eastl::string& virtual_path, const Scene& scene);
  SceneAssetOpResult exportLiveSceneToNewPath(const eastl::string& virtual_path);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  SceneSystem* m_scene_system{nullptr};
  eastl::string m_active_scene_virtual_path;
  bool m_dirty{false};
};

}  // namespace Blunder
