#pragma once

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/scene/scene.h"

namespace Blunder {

class AssetManager;
class SceneAsset;
class SceneInstance;

/// Mesh attach, cameras, then empty-Skeleton hydrate. Shared by SceneSystem and
/// Scene Thumbnail so every scene-document instantiate fills bones (ADR 0034).
void completeSceneDocumentInstantiate(AssetManager* asset_manager,
                                      SceneInstance& instance,
                                      const Scene& scene);

struct SceneSystemInitInfo {
  AssetManager* asset_manager{nullptr};
};

/// Loads scene assets, owns SceneInstance lifetimes, and ticks the active instance.
class SceneSystem final {
 public:
  SceneSystem() = default;

  void initialize(const SceneSystemInitInfo& info);
  void shutdown();

  eastl::shared_ptr<SceneInstance> loadScene(const eastl::string& virtual_path);
  eastl::shared_ptr<SceneInstance> loadGltfScene(const eastl::string& virtual_path);
  void unloadSceneInstance(SceneInstance* instance);

  /// Instantiate a fresh copy of the active scene from disk, then swap.
  /// On failure the current active world stays. Does not rebuild Scripts.
  bool reloadActiveFromDisk();

  void setActiveInstance(SceneInstance* instance);
  SceneInstance* getActiveInstance() const { return m_active_instance; }

  const eastl::vector<eastl::shared_ptr<SceneInstance>>& getLoadedInstances() const {
    return m_loaded_instances;
  }

  void tick(float delta_time);

  /// True when a scene file references meshes but the instance has no renderers yet.
  bool needsMeshAttach(const SceneInstance& instance, const Scene& scene) const;
  /// True when live entities have mesh paths but no mesh renderers in their subtree.
  bool needsMeshAttach(const SceneInstance& instance) const;

 private:
  eastl::shared_ptr<SceneInstance> instantiateScene(
      const eastl::shared_ptr<SceneAsset>& scene_asset,
      const eastl::string& virtual_path);

  AssetManager* m_asset_manager{nullptr};
  eastl::vector<eastl::shared_ptr<SceneInstance>> m_loaded_instances;
  SceneInstance* m_active_instance{nullptr};
  bool m_is_initialized{false};
};

}  // namespace Blunder
