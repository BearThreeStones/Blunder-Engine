#include "runtime/resource/asset_import/editor_mesh_hot_reload.h"

#include <cstring>
#include <exception>

#include "runtime/core/base/macro.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

namespace {

bool endsWithLiteral(const eastl::string& value, const char* suffix) {
  const size_t n = std::strlen(suffix);
  if (value.size() < n) {
    return false;
  }
  return value.compare(value.size() - n, n, suffix) == 0;
}

}  // namespace

void editorMeshHotReloadAfterReimport(const eastl::string& guid,
                                      const eastl::string& descriptor_virtual) {
  if (guid.empty() || descriptor_virtual.empty()) {
    return;
  }
  if (!endsWithLiteral(descriptor_virtual, ".mesh.yaml")) {
    // Clip-only Reimport: Mesh session hot reload not required (ADR 0029).
    return;
  }

  auto& ctx = g_runtime_global_context;
  if (!ctx.m_asset_manager || !ctx.m_asset_registry) {
    return;
  }

  try {
    ctx.m_asset_manager->invalidateMeshCache(descriptor_virtual);
    ctx.m_asset_manager->invalidateMeshCache(guid);

    if (ctx.m_render_system) {
      ctx.m_render_system->invalidateAllGpuMeshes();
    }

    eastl::shared_ptr<MeshAsset> mesh =
        ctx.m_asset_manager->loadMeshByGuid(guid, *ctx.m_asset_registry);
    if (!mesh) {
      LOG_WARN("[MeshHotReload] failed to reload Mesh guid={} path={}",
               guid.c_str(), descriptor_virtual.c_str());
      return;
    }

    if (!ctx.m_scene_system) {
      return;
    }
    SceneInstance* instance = ctx.m_scene_system->getActiveInstance();
    if (instance == nullptr) {
      return;
    }

    eastl::vector<EntityId> to_rebind;
    instance->forEachMeshRenderer(
        [&](EntityId entity_id, const MeshRendererComponent&) {
          const Entity* entity = instance->getEntity(entity_id);
          if (entity == nullptr) {
            return;
          }
          const eastl::string& mesh_path = entity->getMeshVirtualPath();
          if (mesh_path == descriptor_virtual || mesh_path == guid) {
            to_rebind.push_back(entity_id);
            return;
          }
          // Descriptor path vs Intermediate path: also match GUID resolve.
          if (!mesh_path.empty() &&
              ctx.m_asset_registry->findGuidForPath(mesh_path) == guid) {
            to_rebind.push_back(entity_id);
          }
        });

    for (EntityId entity_id : to_rebind) {
      const MeshRendererComponent* previous =
          instance->getMeshRenderer(entity_id);
      MeshRendererComponent renderer =
          previous != nullptr ? *previous : MeshRendererComponent{};
      renderer.mesh = mesh;
      instance->setMeshRenderer(entity_id, eastl::move(renderer));
    }

    LOG_INFO("[MeshHotReload] refreshed {} scene mesh binding(s) for guid={}",
             static_cast<unsigned>(to_rebind.size()), guid.c_str());
  } catch (const std::exception& exception) {
    LOG_WARN("[MeshHotReload] failed soft for guid={}: {}", guid.c_str(),
             exception.what());
  } catch (...) {
    LOG_WARN("[MeshHotReload] failed soft for guid={} (unknown error)",
             guid.c_str());
  }
}

}  // namespace Blunder
