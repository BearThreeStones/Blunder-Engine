#include "runtime/function/editor/placement_preview_controller.h"

#include <glm/gtc/matrix_transform.hpp>

#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

void PlacementPreviewController::ensureLoaded(AssetManager& asset_manager) {
  if (!isVisible()) {
    m_submeshes.clear();
    m_loaded_path.clear();
    return;
  }
  if (!m_submeshes.empty() && m_loaded_path == m_source_path) {
    return;
  }
  m_submeshes = collectMeshPreviewSubmeshes(asset_manager, m_source_path);
  m_loaded_path = m_source_path;
}

void PlacementPreviewController::submitToRender(
    RenderSystem* render_system) const {
  if (!isVisible() || m_submeshes.empty()) {
    return;
  }
  const Mat4 ground = glm::translate(Mat4(1.0f), m_ground_position);
  for (const MeshPreviewSubmeshDraw& draw : m_submeshes) {
    submitStandaloneMeshToRender(render_system, draw.mesh, ground * draw.model);
  }
}

}  // namespace Blunder
