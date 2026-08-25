#include "runtime/function/render/scene_thumbnail/scene_thumbnail_render.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"
#include "runtime/function/render/mesh_preview/mesh_preview_offscreen_backend.h"
#include "runtime/function/scene/gltf_scene_importer.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset/scene_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/platform/file_system/file_system.h"

namespace Blunder {
namespace {

void attachMeshesAndCameras(AssetManager* asset_manager, SceneInstance& instance,
                            const Scene& scene) {
  if (asset_manager == nullptr) {
    return;
  }
  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    if (!definition.mesh_virtual_path.empty()) {
      const EntityId entity_id = instance.findEntityByName(definition.name);
      if (!isValid(entity_id)) {
        continue;
      }
      eastl::string mesh_ref = definition.mesh_virtual_path;
      if (isValidGuidFormat(mesh_ref) &&
          g_runtime_global_context.m_asset_registry) {
        const eastl::string path = asset_manager->resolveGuidPath(
            mesh_ref, *g_runtime_global_context.m_asset_registry);
        if (!path.empty()) {
          mesh_ref = path;
        }
      }
      (void)GltfSceneImporter::importUnderEntity(asset_manager, mesh_ref,
                                                  instance, entity_id);
    }
    if (definition.has_camera) {
      const EntityId entity_id = instance.findEntityByName(definition.name);
      if (isValid(entity_id)) {
        instance.setCamera(entity_id, definition.camera);
      }
    }
  }
}

eastl::shared_ptr<SceneInstance> instantiateScene(
    AssetManager* asset_manager, const eastl::string& virtual_path,
    eastl::vector<eastl::shared_ptr<SceneInstance>>& keep_alive) {
  const eastl::shared_ptr<SceneAsset> scene_asset =
      asset_manager->loadScene(virtual_path);
  if (!scene_asset) {
    return nullptr;
  }

  auto instance = eastl::make_shared<SceneInstance>();
  instance->setSourcePath(virtual_path);
  instance->instantiate(scene_asset->getScene());
  attachMeshesAndCameras(asset_manager, *instance, scene_asset->getScene());

  keep_alive.push_back(instance);
  return instance;
}

void collectDrawsFromInstance(AssetManager& asset_manager,
                              SceneInstance& instance,
                              eastl::vector<MeshPreviewSubmeshDraw>& out) {
  instance.tick(0.0f);
  instance.forEachMeshRenderer([&](EntityId entity_id,
                                   const MeshRendererComponent& renderer) {
    if (!renderer.mesh) {
      return;
    }
    eastl::string path = renderer.mesh->getVirtualPath();
    if (path.empty()) {
      // Fall back to a single draw with identity relative to entity world.
      MeshPreviewSubmeshDraw draw{};
      draw.mesh = renderer.mesh;
      draw.material = renderer.material ? renderer.material
                                        : renderer.mesh->getMaterialAsset();
      draw.model = instance.getWorldMatrix(entity_id);
      draw.entity_id = entity_id;
      out.push_back(eastl::move(draw));
      return;
    }
    eastl::vector<MeshPreviewSubmeshDraw> parts =
        collectMeshPreviewSubmeshes(asset_manager, path);
    const Mat4 world = instance.getWorldMatrix(entity_id);
    if (parts.empty()) {
      MeshPreviewSubmeshDraw draw{};
      draw.mesh = renderer.mesh;
      draw.material = renderer.material ? renderer.material
                                        : renderer.mesh->getMaterialAsset();
      draw.model = world;
      draw.entity_id = entity_id;
      out.push_back(eastl::move(draw));
      return;
    }
    for (MeshPreviewSubmeshDraw& part : parts) {
      part.model = world * part.model;
      part.entity_id = entity_id;
      out.push_back(eastl::move(part));
    }
  });
}

}  // namespace

MeshPreviewCameraFrame meshPreviewFrameFromPlayCamera(
    const Vec3& position, const Vec3& forward, float vertical_fov_radians) {
  MeshPreviewCameraFrame frame{};
  if (!(vertical_fov_radians > 0.0f)) {
    return frame;
  }
  Vec3 dir = forward;
  if (glm::dot(dir, dir) < 1e-8f) {
    dir = Vec3(0.0f, 1.0f, 0.0f);
  } else {
    dir = glm::normalize(dir);
  }
  frame.eye = position;
  frame.target = position + dir;
  frame.up = Vec3(0.0f, 0.0f, 1.0f);
  frame.vertical_fov_rad = vertical_fov_radians;
  frame.ok = true;
  return frame;
}

void SceneThumbnailRenderService::initialize(
    AssetManager* asset_manager, FileSystem* file_system,
    MeshPreviewOffscreenBackend* backend) {
  m_asset_manager = asset_manager;
  m_file_system = file_system;
  m_backend = backend;
  m_is_initialized = asset_manager != nullptr && backend != nullptr;
}

void SceneThumbnailRenderService::shutdown() {
  m_asset_manager = nullptr;
  m_file_system = nullptr;
  m_backend = nullptr;
  m_is_initialized = false;
}

SceneThumbnailRenderResult SceneThumbnailRenderService::renderSceneAsset(
    const SceneThumbnailRenderRequest& request) {
  SceneThumbnailRenderResult result{};
  result.width = request.width;
  result.height = request.height;
  if (!m_is_initialized) {
    result.error = "SceneThumbnailRenderService not initialized";
    return result;
  }
  if (request.scene_virtual_path.empty() || request.width == 0 ||
      request.height == 0) {
    result.error = "Invalid scene thumbnail request";
    return result;
  }

  eastl::vector<eastl::shared_ptr<SceneInstance>> keep_alive;
  const eastl::shared_ptr<SceneInstance> root = instantiateScene(
      m_asset_manager, request.scene_virtual_path, keep_alive);
  if (!root) {
    result.error = "Failed to load scene asset";
    return result;
  }

  root->tick(0.0f);
  const float aspect = static_cast<float>(request.width) /
                       static_cast<float>(request.height);
  const ResolvedPlayCamera camera = resolvePlayCameraFromScene(*root, aspect);
  if (!camera.ok) {
    result.error = "No camera in scene";
    return result;
  }

  const MeshPreviewCameraFrame framing = meshPreviewFrameFromPlayCamera(
      camera.position, camera.forward, camera.vertical_fov_radians);
  if (!framing.ok) {
    result.error = "Invalid camera framing";
    return result;
  }

  eastl::vector<MeshPreviewSubmeshDraw> draws;
  for (eastl::shared_ptr<SceneInstance>& instance : keep_alive) {
    if (instance) {
      collectDrawsFromInstance(*m_asset_manager, *instance, draws);
    }
  }
  if (draws.empty()) {
    result.error = "No mesh renderers in scene";
    return result;
  }

  const MeshPreviewStudioLights lights = defaultMeshPreviewStudioLights();

  if (!m_backend->renderSubmeshDraws(draws, framing, lights, request.width,
                                     request.height, result.rgba, root.get())) {
    result.error = "GPU scene thumbnail render failed";
    result.rgba.clear();
    return result;
  }

  result.ok = true;
  return result;
}

}  // namespace Blunder
