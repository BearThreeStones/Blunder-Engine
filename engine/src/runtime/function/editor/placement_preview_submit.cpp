#include "runtime/function/editor/placement_preview_controller.h"

#include <glm/gtc/matrix_transform.hpp>

#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

void PlacementPreviewController::ensureLoaded(AssetManager& asset_manager) {
  if (!isVisible()) {
    m_mesh.reset();
    m_loaded_path.clear();
    return;
  }
  if (m_mesh && m_loaded_path == m_source_path) {
    return;
  }
  m_mesh = asset_manager.loadMesh(m_source_path);
  m_loaded_path = m_source_path;
}

void PlacementPreviewController::submitToRender(
    RenderSystem* render_system) const {
  if (!isVisible() || m_mesh == nullptr) {
    return;
  }
  const Mat4 world = glm::translate(Mat4(1.0f), m_ground_position);
  submitStandaloneMeshToRender(render_system, m_mesh, world);
}

}  // namespace Blunder
