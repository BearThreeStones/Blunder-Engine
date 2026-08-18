#include "runtime/function/editor/ground_placement.h"

#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/render_system.h"

namespace Blunder {

Vec3 groundPlacementFromWindow(float logical_x, float logical_y) {
  RenderSystem* render_system = g_runtime_global_context.m_render_system.get();
  if (render_system == nullptr) {
    return Vec3(0.0f);
  }

  EditorCamera* camera = render_system->getEditorCamera();
  if (camera == nullptr ||
      !camera->isLogicalPositionInViewport(Vec2(logical_x, logical_y))) {
    return Vec3(0.0f);
  }

  const Ray ray = camera->makeRayFromLogicalPosition(Vec2(logical_x, logical_y));
  return groundPlacementFromRay(ray);
}

}  // namespace Blunder
