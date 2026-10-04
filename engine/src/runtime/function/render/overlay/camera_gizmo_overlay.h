#pragma once

#include <optional>

#include <vulkan/vulkan.h>

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/overlay/camera_gizmo_controller.h"
#include "runtime/function/render/overlay/gizmo_line_batch.h"
#include "runtime/function/render/overlay/overlay_base.h"
#include "runtime/function/render/overlay/overlay_gizmo_pick.h"

namespace Blunder {

class EditorCamera;
class SlangCompiler;

struct OverlayResources;
struct OverlayState;

/// Blender-like wire Camera Gizmo for scene Camera Components.
/// Renders in ScreenOverlayPass (after SSAO) with no depth test.
class CameraGizmoOverlay final : public Overlay {
 public:
  CameraGizmoOverlay() = default;
  ~CameraGizmoOverlay();

  void initialize(const OverlayResources& res, SlangCompiler* compiler);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_screen(VkCommandBuffer cmd, const OverlayState& state) override;

  /// Returns true when the click hit a scene camera gizmo (entity selection).
  bool tryHandleMouseClick(const Vec2& window_position, EditorCamera& camera);

  std::optional<OverlayGizmoPickHit> hitTest(const Vec2& window_position,
                                             EditorCamera& camera) const;

  CameraGizmoController& controller() { return m_controller; }
  const CameraGizmoController& controller() const { return m_controller; }

 private:
  GizmoLineBatch m_batch;
  CameraGizmoController m_controller;
};

}  // namespace Blunder
