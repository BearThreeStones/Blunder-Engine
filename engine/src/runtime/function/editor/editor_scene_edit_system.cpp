#include "runtime/function/editor/editor_scene_edit_system.h"

#include <cstdio>
#include <cstring>

#include <glm/gtc/quaternion.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/math/geometry.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/scene_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include <filesystem>

namespace Blunder {

namespace {

constexpr const char* k_default_scene_path = "assets/Scenes/root.scene.asset";

std::filesystem::path resolveSceneAssetAbsolute(FileSystem* file_system,
                                                const eastl::string& virtual_path) {
  if (file_system == nullptr || virtual_path.empty()) {
    return {};
  }
  const eastl::string prefix_assets = "assets/";
  if (virtual_path.size() > prefix_assets.size() &&
      virtual_path.compare(0, prefix_assets.size(), prefix_assets) == 0) {
    const eastl::string relative =
        virtual_path.substr(prefix_assets.size(), virtual_path.size());
    return file_system->resolveAsset(std::filesystem::path(relative.c_str()));
  }
  return file_system->resolveAsset(std::filesystem::path(virtual_path.c_str()));
}

bool endsWithSuffix(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

eastl::string entityStemFromAssetPath(const eastl::string& virtual_path) {
  const size_t slash = virtual_path.find_last_of('/');
  const eastl::string file_name =
      slash == eastl::string::npos
          ? virtual_path
          : virtual_path.substr(slash + 1, virtual_path.size());
  const size_t dot = file_name.find_last_of('.');
  if (dot == eastl::string::npos) {
    return file_name;
  }
  return file_name.substr(0, dot);
}

eastl::string makeUniqueEntityName(SceneInstance& instance,
                                   const eastl::string& stem) {
  if (instance.findEntityByName(stem) == k_invalid_entity_id) {
    return stem;
  }

  for (uint32_t index = 1; index < 10000; ++index) {
    char candidate[128];
    std::snprintf(candidate, sizeof(candidate), "%s_%u", stem.c_str(), index);
    if (instance.findEntityByName(eastl::string(candidate)) ==
        k_invalid_entity_id) {
      return eastl::string(candidate);
    }
  }
  return stem;
}

Vec3 groundPositionFromWindow(float window_x, float window_y) {
  RenderSystem* render_system = g_runtime_global_context.m_render_system.get();
  if (render_system == nullptr) {
    return Vec3(0.0f);
  }

  EditorCamera* camera = render_system->getEditorCamera();
  if (camera == nullptr || !camera->isWindowPositionInViewport(
                               Vec2(window_x, window_y))) {
    return Vec3(0.0f);
  }

  const Ray ray = camera->makeRayFromWindowPosition(Vec2(window_x, window_y));
  const Plane ground =
      Plane::fromPointAndNormal(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
  const std::optional<RayHit> hit = intersect(ray, ground);
  if (!hit.has_value()) {
    return Vec3(0.0f);
  }
  return hit->point;
}

}  // namespace

void EditorSceneEditSystem::initialize(FileSystem* file_system,
                                       AssetManager* asset_manager,
                                       SceneSystem* scene_system) {
  m_file_system = file_system;
  m_asset_manager = asset_manager;
  m_scene_system = scene_system;
}

void EditorSceneEditSystem::setActiveScenePath(eastl::string virtual_path) {
  m_active_scene_virtual_path = eastl::move(virtual_path);
  m_dirty = false;
}

bool EditorSceneEditSystem::openScene(const eastl::string& virtual_path) {
  if (!m_scene_system || virtual_path.empty()) {
    return false;
  }

  SceneInstance* active = m_scene_system->getActiveInstance();
  if (active != nullptr && active->getSourcePath() == virtual_path) {
    if (m_asset_manager) {
      const eastl::shared_ptr<SceneAsset> scene_asset =
          m_asset_manager->loadScene(virtual_path);
      if (scene_asset &&
          m_scene_system->needsMeshAttach(*active, scene_asset->getScene())) {
        LOG_WARN(
            "[EditorSceneEdit] reloading '{}' — scene file has mesh descriptors "
            "but runtime instance has no mesh renderers",
            virtual_path.c_str());
        m_scene_system->unloadSceneInstance(active);
        active = nullptr;
      }
    }
    if (active != nullptr) {
      setActiveScenePath(virtual_path);
      return true;
    }
  }

  const eastl::shared_ptr<SceneInstance> instance =
      m_scene_system->loadScene(virtual_path);
  if (!instance) {
    LOG_ERROR("[EditorSceneEdit] failed to open scene '{}'", virtual_path.c_str());
    return false;
  }

  m_scene_system->setActiveInstance(instance.get());
  setActiveScenePath(virtual_path);

  if (g_runtime_global_context.m_editor_selection) {
    g_runtime_global_context.m_editor_selection->clearSelection();
  }
  if (g_runtime_global_context.m_hierarchy) {
    g_runtime_global_context.m_hierarchy->clearExpanded();
    g_runtime_global_context.m_hierarchy->rebuildVisibleTree(instance.get());
    g_runtime_global_context.m_hierarchy->markDirty();
  }

  LOG_INFO("[EditorSceneEdit] opened scene '{}'", virtual_path.c_str());
  return true;
}

bool EditorSceneEditSystem::saveActiveScene() {
  if (!m_file_system || !m_scene_system || m_active_scene_virtual_path.empty()) {
    LOG_ERROR("[EditorSceneEdit] save failed: system or path not ready");
    return false;
  }

  SceneInstance* instance = m_scene_system->getActiveInstance();
  if (instance == nullptr) {
    LOG_ERROR("[EditorSceneEdit] save failed: no active scene instance");
    return false;
  }

  Scene scene;
  if (!instance->exportToScene(scene)) {
    LOG_ERROR("[EditorSceneEdit] exportToScene failed");
    return false;
  }

  if (m_asset_manager) {
    const eastl::shared_ptr<SceneAsset> asset =
        m_asset_manager->loadScene(m_active_scene_virtual_path);
    if (asset) {
      scene.getChildScenes() = asset->getScene().getChildScenes();
      if (!scene.getName().empty()) {
        scene.setName(asset->getScene().getName());
      } else if (!asset->getScene().getName().empty()) {
        scene.setName(asset->getScene().getName());
      }
    }
  }

  eastl::string json_text;
  if (!SceneSerializer::serialize(scene, json_text)) {
    LOG_ERROR("[EditorSceneEdit] serialize failed");
    return false;
  }

  const std::filesystem::path absolute =
      resolveSceneAssetAbsolute(m_file_system, m_active_scene_virtual_path);
  if (absolute.empty() || !m_file_system->writeText(absolute, json_text)) {
    LOG_ERROR("[EditorSceneEdit] write failed: {}",
              absolute.generic_string().c_str());
    return false;
  }

  m_dirty = false;
  LOG_INFO("[EditorSceneEdit] saved '{}'", m_active_scene_virtual_path.c_str());
  return true;
}

SpawnAssetResult EditorSceneEditSystem::spawnMeshAsset(
    const eastl::string& asset_virtual_path, float window_x, float window_y) {
  SpawnAssetResult result{};
  if (!m_asset_manager || !m_scene_system) {
    return result;
  }

  if (m_scene_system->getActiveInstance() == nullptr) {
    if (!openScene(eastl::string(k_default_scene_path))) {
      LOG_ERROR("[EditorSceneEdit] spawn failed: no active scene");
      return result;
    }
  }

  SceneInstance* instance = m_scene_system->getActiveInstance();
  if (instance == nullptr) {
    return result;
  }

  const eastl::shared_ptr<MeshAsset> mesh =
      m_asset_manager->loadMesh(asset_virtual_path);
  if (!mesh) {
    LOG_ERROR("[EditorSceneEdit] spawn failed: cannot load mesh '{}'",
              asset_virtual_path.c_str());
    return result;
  }

  const Vec3 position = groundPositionFromWindow(window_x, window_y);
  const eastl::string entity_name =
      makeUniqueEntityName(*instance, entityStemFromAssetPath(asset_virtual_path));
  const EntityId entity_id = instance->createEntity(
      entity_name, position, glm::identity<Quat>(), Vec3(1.0f));

  MeshRendererComponent renderer{};
  renderer.mesh = mesh;
  renderer.material = mesh->getMaterialAsset();
  if (renderer.material) {
    renderer.alpha_mode = renderer.material->getAlphaMode();
    renderer.alpha_cutoff = renderer.material->getAlphaCutoff();
    renderer.double_sided = renderer.material->isDoubleSided();
  }
  instance->setMeshRenderer(entity_id, eastl::move(renderer));
  markDirty();

  if (g_runtime_global_context.m_editor_selection) {
    g_runtime_global_context.m_editor_selection->setSelection(entity_id);
  }
  if (g_runtime_global_context.m_hierarchy) {
    g_runtime_global_context.m_hierarchy->rebuildVisibleTree(instance);
    g_runtime_global_context.m_hierarchy->markDirty();
  }

  result.success = true;
  result.spawned_entity = entity_id;
  LOG_INFO("[EditorSceneEdit] spawned '{}' at ({}, {}, {})",
           entity_name.c_str(), position.x, position.y, position.z);
  return result;
}

SpawnAssetResult EditorSceneEditSystem::spawnAssetAtWindowPosition(
    const eastl::string& asset_virtual_path, float window_x, float window_y) {
  SpawnAssetResult result{};
  if (asset_virtual_path.empty()) {
    return result;
  }

  if (endsWithSuffix(asset_virtual_path, ".scene.asset")) {
    result.success = openScene(asset_virtual_path);
    return result;
  }

  if (endsWithSuffix(asset_virtual_path, ".mesh.yaml") ||
      endsWithSuffix(asset_virtual_path, ".mesh.asset")) {
    return spawnMeshAsset(asset_virtual_path, window_x, window_y);
  }

  LOG_WARN("[EditorSceneEdit] spawn unsupported asset type: {}",
           asset_virtual_path.c_str());
  return result;
}

}  // namespace Blunder
