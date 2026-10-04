#pragma once

#include <optional>

#include <vulkan/vulkan.h>

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/overlay/gizmo_line_batch.h"
#include "runtime/function/render/overlay/overlay_base.h"
#include "runtime/function/render/overlay/overlay_gizmo_pick.h"

namespace Blunder {

class EditorCamera;
class SlangCompiler;

struct OverlayResources;
struct OverlayState;

/// Type-shaped Light Gizmo wires for scene Light Components.
class LightGizmoOverlay final : public Overlay {
 public:
  LightGizmoOverlay() = default;
  ~LightGizmoOverlay();

  void initialize(const OverlayResources& res, SlangCompiler* compiler);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_screen(VkCommandBuffer cmd, const OverlayState& state) override;

  std::optional<OverlayGizmoPickHit> hitTest(const Vec2& window_position,
                                             EditorCamera& camera) const;

 private:
  GizmoLineBatch m_batch;
};

}  // namespace Blunder
