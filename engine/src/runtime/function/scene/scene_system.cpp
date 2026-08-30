#include "runtime/function/scene/scene_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/gltf_scene_importer.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/script/dotnet_host.h"
#include "runtime/function/script/scene_behaviour_mount.h"
#include "runtime/resource/asset/scene_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/function/scene/skeleton_from_gltf.h"

namespace Blunder {

namespace {

bool isEntityOrDescendant(const SceneInstance& instance, EntityId root,
                          EntityId candidate) {
  EntityId current = candidate;
  while (isValid(current)) {
    if (current == root) {
      return true;
    }
    const Entity* entity = instance.getEntity(current);
    if (entity == nullptr) {
      break;
    }
    current = entity->getParentId();
  }
  return false;
}

bool entitySubtreeHasMeshRenderer(const SceneInstance& instance, EntityId root) {
  bool found = false;
  instance.forEachMeshRenderer([&](EntityId entity_id,
                                   const MeshRendererComponent& renderer) {
    if (found || !renderer.mesh) {
      return;
    }
    if (isEntityOrDescendant(instance, root, entity_id)) {
      found = true;
    }
  });
  return found;
}

}  // namespace

bool SceneSystem::needsMeshAttach(const SceneInstance& instance,
                                  const Scene& scene) const {
  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    if (definition.mesh_virtual_path.empty()) {
      continue;
    }
    const EntityId entity_id = instance.findEntityByName(definition.name);
    if (!isValid(entity_id)) {
      return true;
    }
    if (!entitySubtreeHasMeshRenderer(instance, entity_id)) {
      return true;
    }
  }
  return false;
}

bool SceneSystem::needsMeshAttach(const SceneInstance& instance) const {
  bool needed = false;
  instance.forEachEntity([&](EntityId id, const Entity& entity) {
    if (needed || entity.isTombstoned() || entity.getMeshVirtualPath().empty()) {
      return;
    }
    if (!entitySubtreeHasMeshRenderer(instance, id)) {
      needed = true;
    }
  });
  return needed;
}

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
    const eastl::string& virtual_path) {
  if (!scene_asset) {
    return nullptr;
  }

  auto instance = eastl::make_shared<SceneInstance>();
  instance->setSourcePath(virtual_path);
  instance->instantiate(scene_asset->getScene());

  if (g_runtime_global_context.m_dotnet_host != nullptr) {
    mountSceneBehaviours(*instance, *g_runtime_global_context.m_dotnet_host,
                         &scene_asset->getScene());
  }

  attachSceneEntityMeshes(*instance, scene_asset->getScene());
  attachSceneEntityCameras(*instance, scene_asset->getScene());
  hydrateEmptySkeletonsFromEntityMeshes(m_asset_manager, *instance);

  return instance;
}

void SceneSystem::attachSceneEntityMeshes(SceneInstance& instance,
                                          const Scene& scene) {
  GltfSceneImporter::attachEntityMeshes(m_asset_manager, instance, scene);
}

void SceneSystem::attachSceneEntityCameras(SceneInstance& instance,
                                           const Scene& scene) {
  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    if (!definition.has_camera) {
      continue;
    }

    const EntityId entity_id = instance.findEntityByName(definition.name);
    if (!isValid(entity_id)) {
      LOG_WARN("[SceneSystem] camera entity '{}' not found in scene '{}'",
               definition.name.c_str(), instance.getSourcePath().c_str());
      continue;
    }

    instance.setCamera(entity_id, definition.camera);
    LOG_INFO("[SceneSystem] attached camera to entity '{}' in '{}'",
             definition.name.c_str(), instance.getSourcePath().c_str());
  }
}

eastl::shared_ptr<SceneInstance> SceneSystem::loadScene(
    const eastl::string& virtual_path) {
  if (!m_is_initialized || !m_asset_manager) {
    LOG_ERROR("[SceneSystem] loadScene before initialize()");
    return nullptr;
  }

  for (auto it = m_loaded_instances.begin(); it != m_loaded_instances.end();) {
    const eastl::shared_ptr<SceneInstance>& existing = *it;
    if (!existing || existing->getSourcePath() != virtual_path) {
      ++it;
      continue;
    }

    if (!needsMeshAttach(*existing)) {
      LOG_WARN("[SceneSystem] scene '{}' already loaded, returning existing instance",
               virtual_path.c_str());
      return existing;
    }

    LOG_WARN(
        "[SceneSystem] reloading scene '{}' to attach mesh descriptors from scene file",
        virtual_path.c_str());
    if (m_active_instance == existing.get()) {
      m_active_instance = nullptr;
    }
    it = m_loaded_instances.erase(it);
  }

  const eastl::shared_ptr<SceneAsset> scene_asset =
      m_asset_manager->loadScene(virtual_path);
  if (!scene_asset) {
    LOG_ERROR("[SceneSystem] failed to load scene asset '{}'", virtual_path.c_str());
    return nullptr;
  }

  const eastl::shared_ptr<SceneInstance> root_instance =
      instantiateScene(scene_asset, virtual_path);
  if (!root_instance) {
    return nullptr;
  }

  m_loaded_instances.push_back(root_instance);
  LOG_INFO("[SceneSystem] loaded scene '{}' (entities={})",
           virtual_path.c_str(), root_instance->getEntityCount());
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

bool SceneSystem::reloadActiveFromDisk() {
  if (!m_is_initialized || !m_asset_manager || m_active_instance == nullptr) {
    LOG_ERROR("[SceneSystem] reloadActiveFromDisk with no active scene");
    return false;
  }

  SceneInstance* old = m_active_instance;
  const eastl::string path = old->getSourcePath();
  if (path.empty()) {
    LOG_ERROR("[SceneSystem] reloadActiveFromDisk: active scene has empty path");
    return false;
  }

  m_asset_manager->invalidateSceneCache(path);
  const eastl::shared_ptr<SceneAsset> scene_asset =
      m_asset_manager->loadScene(path);
  if (!scene_asset) {
    LOG_ERROR("[SceneSystem] reloadActiveFromDisk: failed to load '{}'",
              path.c_str());
    return false;
  }

  const eastl::shared_ptr<SceneInstance> neu =
      instantiateScene(scene_asset, path);
  if (!neu) {
    LOG_ERROR("[SceneSystem] reloadActiveFromDisk: instantiate failed for '{}'",
              path.c_str());
    return false;
  }

  m_loaded_instances.push_back(neu);
  setActiveInstance(neu.get());
  unloadSceneInstance(old);
  LOG_INFO("[SceneSystem] reloaded scene '{}' (entities={})", path.c_str(),
           neu->getEntityCount());
  return true;
}

void SceneSystem::unloadSceneInstance(SceneInstance* instance) {
  if (!m_is_initialized || instance == nullptr) {
    return;
  }

  if (m_active_instance == instance) {
    m_active_instance = nullptr;
  }

  for (auto it = m_loaded_instances.begin(); it != m_loaded_instances.end();) {
    if (it->get() == instance) {
      LOG_INFO("[SceneSystem] unloaded scene '{}'",
               instance->getSourcePath().c_str());
      it = m_loaded_instances.erase(it);
      break;
    }
    ++it;
  }
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
