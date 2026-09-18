#include "runtime/function/scene/scene_system.h"

#include <chrono>

#include "EASTL/unordered_set.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/entity.h"
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

void coverMeshRendererAncestors(const SceneInstance& instance,
                                eastl::unordered_set<EntityId>& covered) {
  instance.forEachMeshRenderer([&](EntityId entity_id,
                                   const MeshRendererComponent& renderer) {
    if (!renderer.mesh) {
      return;
    }
    EntityId current = entity_id;
    while (isValid(current) && covered.insert(current).second) {
      const Entity* entity = instance.getEntity(current);
      if (entity == nullptr) {
        break;
      }
      current = entity->getParentId();
    }
  });
}

}  // namespace

bool SceneSystem::needsMeshAttach(const SceneInstance& instance,
                                  const Scene& scene) const {
  eastl::unordered_set<EntityId> covered;
  coverMeshRendererAncestors(instance, covered);
  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    if (definition.mesh_virtual_path.empty()) {
      continue;
    }
    const EntityId entity_id = instance.findEntityByName(definition.name);
    if (!isValid(entity_id) || covered.find(entity_id) == covered.end()) {
      return true;
    }
  }
  return false;
}

bool SceneSystem::needsMeshAttach(const SceneInstance& instance) const {
  eastl::unordered_set<EntityId> covered;
  coverMeshRendererAncestors(instance, covered);
  bool needed = false;
  instance.forEachEntity([&](EntityId id, const Entity& entity) {
    if (needed || entity.isTombstoned() || entity.getMeshVirtualPath().empty()) {
      return;
    }
    if (covered.find(id) == covered.end()) {
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
  setActiveInstance(nullptr);
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
  const auto instantiate_begin = std::chrono::steady_clock::now();
  instance->instantiate(scene_asset->getScene());
  const double instantiate_ms =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - instantiate_begin)
          .count();

  if (g_runtime_global_context.m_dotnet_host != nullptr) {
    mountSceneBehaviours(*instance, *g_runtime_global_context.m_dotnet_host,
                         &scene_asset->getScene());
  }

  const auto attach_begin = std::chrono::steady_clock::now();
  completeSceneDocumentInstantiate(m_asset_manager, *instance,
                                   scene_asset->getScene());
  const double attach_ms =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - attach_begin)
          .count();
  LOG_INFO(
      "[SceneSystem] instantiate '{}' entities={} graph={:.1f}ms attach={:.1f}ms",
      virtual_path.c_str(), instance->getEntityCount(), instantiate_ms,
      attach_ms);

  return instance;
}

void completeSceneDocumentInstantiate(AssetManager* asset_manager,
                                      SceneInstance& instance,
                                      const Scene& scene) {
  if (asset_manager != nullptr) {
    GltfSceneImporter::attachEntityMeshes(asset_manager, instance, scene);
  }

  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    if (!definition.has_camera) {
      continue;
    }

    const EntityId entity_id = instance.findEntityByName(definition.name);
    if (!isValid(entity_id)) {
      LOG_WARN("[SceneInstantiate] camera entity '{}' not found in scene '{}'",
               definition.name.c_str(), instance.getSourcePath().c_str());
      continue;
    }

    instance.setCamera(entity_id, definition.camera);
    LOG_INFO("[SceneInstantiate] attached camera to entity '{}' in '{}'",
             definition.name.c_str(), instance.getSourcePath().c_str());
  }

  if (asset_manager != nullptr) {
    hydrateEmptySkeletonsFromEntityMeshes(asset_manager, instance);
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
      setActiveInstance(nullptr);
    }
    it = m_loaded_instances.erase(it);
  }

  const auto load_begin = std::chrono::steady_clock::now();
  const eastl::shared_ptr<SceneAsset> scene_asset =
      m_asset_manager->loadScene(virtual_path);
  if (!scene_asset) {
    LOG_ERROR("[SceneSystem] failed to load scene asset '{}'", virtual_path.c_str());
    return nullptr;
  }
  const double deserialize_ms =
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - load_begin)
          .count();

  const eastl::shared_ptr<SceneInstance> root_instance =
      instantiateScene(scene_asset, virtual_path);
  if (!root_instance) {
    return nullptr;
  }

  m_loaded_instances.push_back(root_instance);
  LOG_INFO(
      "[SceneSystem] loaded scene '{}' (entities={}, deserialize={:.1f}ms)",
      virtual_path.c_str(), root_instance->getEntityCount(), deserialize_ms);
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
    setActiveInstance(nullptr);
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
  if (m_active_instance != instance) {
    if (g_runtime_global_context.m_render_system) {
      g_runtime_global_context.m_render_system->dropInFlightTextures();
      g_runtime_global_context.m_render_system->notifyActiveSceneChanged();
    }
  }
  m_active_instance = instance;
  ObjectDB::setEntityStore(instance);
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
