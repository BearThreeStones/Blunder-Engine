#include "runtime/function/editor/placement_preview_controller.h"

#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/content_browser/content_browser_drop.h"

namespace Blunder {

void PlacementPreviewController::setSourcePath(eastl::string virtual_path) {
  if (m_source_path == virtual_path) {
    return;
  }
  m_source_path = eastl::move(virtual_path);
  m_mesh.reset();
  m_loaded_path.clear();
}

void PlacementPreviewController::setPointerOverViewport(bool over_viewport) {
  m_over_viewport = over_viewport;
}

void PlacementPreviewController::setGroundPosition(const Vec3& position) {
  m_ground_position = position;
}

void PlacementPreviewController::clear() {
  m_source_path.clear();
  m_loaded_path.clear();
  m_ground_position = Vec3(0.0f);
  m_over_viewport = false;
  m_mesh.reset();
}

bool PlacementPreviewController::isVisible() const {
  return m_over_viewport &&
         classifyContentBrowserDrop(m_source_path) == ContentBrowserDropKind::mesh;
}

}  // namespace Blunder
