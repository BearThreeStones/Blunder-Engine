#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "runtime/function/render/overlay/axes_overlay.h"
#include "runtime/function/render/overlay/grid_overlay.h"
#include "runtime/function/render/gizmo/transform_gizmo_overlay.h"
#include "runtime/function/render/overlay/navigate_gizmo_overlay.h"
#include "runtime/function/render/overlay/origins_overlay.h"
#include "runtime/function/render/overlay/outline_overlay.h"
#include "runtime/function/render/overlay/outline_targets.h"
#include "runtime/function/render/overlay/overlay_anti_aliasing.h"
#include "runtime/function/render/overlay/overlay_line_pass.h"
#include "runtime/function/render/overlay/overlay_line_targets.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/overlay/screen_overlay_pass.h"
#include "runtime/function/render/overlay/wireframe_overlay.h"

namespace Blunder {

class OffscreenRenderTarget;
class SlangCompiler;
class VulkanAllocator;
class VulkanContext;

namespace rhi {
class IOffscreenRenderTarget;
}  // namespace rhi

struct ForwardFrameState;

/// Orchestrates overlay rendering in ordered phases:
///   1. draw_scene_overlays — inside forward pass (depth-aware solids)
///   2. draw_overlay_lines — MRT line capture (OverlayLinePass)
///   3. draw_overlay_aa — composite AA lines onto main color
///   4. draw_screen_overlays — after SSAO (ScreenOverlayPass: grid + transform + nav gizmo)
class OverlaySystem final {
 public:
  OverlaySystem() = default;
  ~OverlaySystem();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                  rhi::IOffscreenRenderTarget* offscreen,
                  SlangCompiler* compiler);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  void begin_sync(const ForwardFrameState& frame_state,
                  uint32_t current_frame);

  bool hasActiveLineOverlays() const;
  bool hasActiveOutline() const;

  void draw_scene_overlays(VkCommandBuffer cmd);
  void draw_outline(VkCommandBuffer cmd);
  void draw_overlay_lines(VkCommandBuffer cmd);
  void draw_overlay_aa(VkCommandBuffer cmd);
  void draw_screen_overlays(VkCommandBuffer cmd);

  GridOverlay& grid() { return m_grid; }
  AxesOverlay& axes() { return m_axes; }
  WireframeOverlay& wireframe() { return m_wireframe; }
  OutlineOverlay& outline() { return m_outline; }
  OriginsOverlay& origins() { return m_origins; }
  NavigateGizmoOverlay& navigate_gizmo() { return m_navigate_gizmo; }
  TransformGizmoOverlay& transform_gizmo() { return m_transform_gizmo; }
  OverlayAntiAliasing& anti_aliasing() { return m_anti_aliasing; }

 private:
  OverlayState m_state;
  OverlayResources m_resources;
  OffscreenRenderTarget* m_native_offscreen{nullptr};

  ScreenOverlayPass m_screen_pass;
  OverlayLineTargets m_line_targets;
  OverlayLinePass m_line_pass;

  OutlineTargets m_outline_targets;
  OutlineOverlay m_outline;

  GridOverlay m_grid;
  AxesOverlay m_axes;
  WireframeOverlay m_wireframe;
  OriginsOverlay m_origins;
  NavigateGizmoOverlay m_navigate_gizmo;
  TransformGizmoOverlay m_transform_gizmo;
  OverlayAntiAliasing m_anti_aliasing;
};

}  // namespace Blunder
