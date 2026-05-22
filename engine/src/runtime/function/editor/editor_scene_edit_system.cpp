#include "runtime/function/editor/editor_scene_edit_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/scene_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include <filesystem>

namespace Blunder {

namespace {

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
    setActiveScenePath(virtual_path);
    return true;
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

}  // namespace Blunder
