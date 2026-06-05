#pragma once

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/scene/scene.h"

namespace Blunder {

class AssetManager;
class SceneAsset;
class SceneInstance;

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

  void setActiveInstance(SceneInstance* instance);
  SceneInstance* getActiveInstance() const { return m_active_instance; }

  const eastl::vector<eastl::shared_ptr<SceneInstance>>& getLoadedInstances() const {
    return m_loaded_instances;
  }

  void tick(float delta_time);

  /// True when a scene file references meshes but the instance has no renderers yet.
  bool needsMeshAttach(const SceneInstance& instance, const Scene& scene) const;

 private:
  eastl::shared_ptr<SceneInstance> instantiateScene(
      const eastl::shared_ptr<SceneAsset>& scene_asset,
      const eastl::string& virtual_path, SceneInstance* parent_instance,
      const SceneChildReference* child_reference);

  void unloadSceneInstanceRecursive(SceneInstance* instance);

  void attachSceneEntityMeshes(SceneInstance& instance, const Scene& scene);

  AssetManager* m_asset_manager{nullptr};
  eastl::vector<eastl::shared_ptr<SceneInstance>> m_loaded_instances;
  SceneInstance* m_active_instance{nullptr};
  bool m_is_initialized{false};
};

}  // namespace Blunder
