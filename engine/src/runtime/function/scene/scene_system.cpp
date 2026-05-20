#include "runtime/function/scene/scene_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/scene/gltf_scene_importer.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/scene_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

void SceneSystem::initialize(const SceneSystemInitInfo& info) {
  m_asset_manager = info.asset_manager;
  m_is_initialized = m_asset_manager != nullptr;
  if (!m_is_initialized) {
    LOG_ERROR("[SceneSystem] initialize requires AssetManager");
  }
}

void SceneSystem::shutdown() {
  m_active_instance = nullptr;
  m_loaded_instances.clear();
  m_asset_manager = nullptr;
  m_is_initialized = false;
}

eastl::shared_ptr<SceneInstance> SceneSystem::instantiateScene(
    const eastl::shared_ptr<SceneAsset>& scene_asset,
    const eastl::string& virtual_path, SceneInstance* parent_instance,
    const SceneChildReference* child_reference) {
  if (!scene_asset) {
    return nullptr;
  }

  auto instance = eastl::make_shared<SceneInstance>();
  instance->setSourcePath(virtual_path);
  instance->instantiate(scene_asset->getScene());

  if (child_reference != nullptr) {
    instance->setRootTransform(child_reference->position, child_reference->rotation,
                             child_reference->scale);
  }

  if (parent_instance != nullptr) {
    instance->setParent(parent_instance);
    LOG_INFO("[SceneSystem] child scene '{}' parent -> '{}'",
             virtual_path.c_str(), parent_instance->getSourcePath().c_str());
  }

  for (const SceneChildReference& child : scene_asset->getScene().getChildScenes()) {
    const eastl::shared_ptr<SceneAsset> child_asset =
        m_asset_manager->loadScene(child.scene_virtual_path);
    if (!child_asset) {
      LOG_ERROR("[SceneSystem] failed to load child scene '{}' for parent '{}'",
                child.scene_virtual_path.c_str(), virtual_path.c_str());
      continue;
    }

    const eastl::shared_ptr<SceneInstance> child_instance = instantiateScene(
        child_asset, child.scene_virtual_path, instance.get(), &child);
    if (child_instance) {
      m_loaded_instances.push_back(child_instance);
    }
  }

  return instance;
}

eastl::shared_ptr<SceneInstance> SceneSystem::loadScene(
    const eastl::string& virtual_path) {
  if (!m_is_initialized || !m_asset_manager) {
    LOG_ERROR("[SceneSystem] loadScene before initialize()");
    return nullptr;
  }

  for (const eastl::shared_ptr<SceneInstance>& existing : m_loaded_instances) {
    if (existing && existing->getSourcePath() == virtual_path) {
      LOG_WARN("[SceneSystem] scene '{}' already loaded, returning existing instance",
               virtual_path.c_str());
      return existing;
    }
  }

  const eastl::shared_ptr<SceneAsset> scene_asset =
      m_asset_manager->loadScene(virtual_path);
  if (!scene_asset) {
    LOG_ERROR("[SceneSystem] failed to load scene asset '{}'", virtual_path.c_str());
    return nullptr;
  }

  const eastl::shared_ptr<SceneInstance> root_instance =
      instantiateScene(scene_asset, virtual_path, nullptr, nullptr);
  if (!root_instance) {
    return nullptr;
  }

  m_loaded_instances.push_back(root_instance);
  LOG_INFO("[SceneSystem] loaded scene '{}' (entities={}, child_refs={})",
           virtual_path.c_str(), root_instance->getEntityCount(),
           scene_asset->getScene().getChildScenes().size());

  return root_instance;
}

eastl::shared_ptr<SceneInstance> SceneSystem::loadGltfScene(
    const eastl::string& virtual_path) {
  if (!m_is_initialized || !m_asset_manager) {
    LOG_ERROR("[SceneSystem] loadGltfScene before initialize()");
    return nullptr;
  }

  for (const eastl::shared_ptr<SceneInstance>& existing : m_loaded_instances) {
    if (existing && existing->getSourcePath() == virtual_path) {
      LOG_WARN(
          "[SceneSystem] glTF scene '{}' already loaded, returning existing instance",
          virtual_path.c_str());
      return existing;
    }
  }

  auto instance = eastl::make_shared<SceneInstance>();
  const GltfSceneImporter::ImportResult import_result =
      GltfSceneImporter::importIntoScene(m_asset_manager, virtual_path, *instance);
  if (!import_result.success) {
    LOG_ERROR("[SceneSystem] failed to import glTF '{}': {}", virtual_path.c_str(),
              import_result.error_message.c_str());
    return nullptr;
  }

  m_loaded_instances.push_back(instance);
  LOG_INFO("[SceneSystem] loaded glTF '{}' (primitives={})",
           virtual_path.c_str(), import_result.mesh_primitive_count);
  return instance;
}

void SceneSystem::unloadSceneInstanceRecursive(SceneInstance* instance) {
  if (instance == nullptr) {
    return;
  }

  eastl::vector<SceneInstance*> children;
  children.reserve(m_loaded_instances.size());
  for (const eastl::shared_ptr<SceneInstance>& candidate : m_loaded_instances) {
    if (candidate && candidate->getParent() == instance) {
      children.push_back(candidate.get());
    }
  }
  for (SceneInstance* child : children) {
    unloadSceneInstanceRecursive(child);
  }

  if (m_active_instance == instance) {
    m_active_instance = nullptr;
  }

  for (auto it = m_loaded_instances.begin(); it != m_loaded_instances.end();) {
    if (it->get() == instance) {
      LOG_INFO("[SceneSystem] unloaded scene '{}'", instance->getSourcePath().c_str());
      it = m_loaded_instances.erase(it);
      break;
    }
  }
}

void SceneSystem::unloadSceneInstance(SceneInstance* instance) {
  if (!m_is_initialized || instance == nullptr) {
    return;
  }
  unloadSceneInstanceRecursive(instance);
}

void SceneSystem::setActiveInstance(SceneInstance* instance) {
  m_active_instance = instance;
  if (instance != nullptr) {
    LOG_INFO("[SceneSystem] active scene set to '{}'", instance->getSourcePath().c_str());
  }
}

void SceneSystem::tick(float delta_time) {
  if (m_active_instance == nullptr) {
    return;
  }
  m_active_instance->tick(delta_time);
}

}  // namespace Blunder
