#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

#include "runtime/function/render/overlay/axes_overlay.h"
#include "runtime/function/render/overlay/grid_overlay.h"
#include "runtime/function/render/gizmo/transform_gizmo_overlay.h"
#include "runtime/function/render/overlay/navigate_gizmo_overlay.h"
#include "runtime/function/render/overlay/origins_overlay.h"
#include "runtime/function/render/overlay/outline_overlay.h"
#include "runtime/function/render/overlay/outline_targets.h"
#include "runtime/function/render/overlay/pick_overlay.h"
#include "runtime/function/render/pick/hybrid_gpu_pick_system.h"
#include "runtime/function/render/pick/pick_instance_buffer.h"
#include "runtime/function/render/overlay/pick_targets.h"
#include "runtime/function/render/overlay/overlay_anti_aliasing.h"
#include "runtime/function/render/overlay/overlay_line_pass.h"
#include "runtime/function/render/overlay/overlay_line_targets.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/overlay/screen_overlay_pass.h"
#include "runtime/function/render/overlay/wireframe_overlay.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class EditorCamera;
class OffscreenRenderTarget;
class RenderSystem;
class SceneInstance;
class SlangCompiler;
class ViewportPickSystem;
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

  EntityId pickAtWindowPosition(float window_x, float window_y, EditorCamera& camera,
                                SceneInstance& scene, RenderSystem& render_system);

  eastl::vector<EntityId> pickAllAtWindowPosition(float window_x, float window_y,
                                                  EditorCamera& camera,
                                                  SceneInstance& scene,
                                                  RenderSystem& render_system);

  HybridGpuPickSystem& hybridPick() { return m_hybrid_pick; }
  const HybridGpuPickSystem& hybridPick() const { return m_hybrid_pick; }

  void markPickInstancesDirty() { m_pick_instances_dirty = true; }
  void rebuildPickInstancesIfNeeded(SceneInstance& scene, RenderSystem& render_system);
  const PickInstanceBuffer& pickInstances() const { return m_pick_instances; }

  void pollHybridPick(EditorCamera& camera, SceneInstance& scene,
                      RenderSystem& render_system, ViewportPickSystem& viewport_pick);

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

  PickTargets m_pick_targets;
  PickOverlay m_pick;
  HybridGpuPickSystem m_hybrid_pick;
  PickInstanceBuffer m_pick_instances;
  bool m_pick_instances_dirty{true};

  GridOverlay m_grid;
  AxesOverlay m_axes;
  WireframeOverlay m_wireframe;
  OriginsOverlay m_origins;
  NavigateGizmoOverlay m_navigate_gizmo;
  TransformGizmoOverlay m_transform_gizmo;
  OverlayAntiAliasing m_anti_aliasing;
};

}  // namespace Blunder
