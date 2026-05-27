#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "runtime/function/render/overlay/axes_overlay.h"
#include "runtime/function/render/overlay/grid_overlay.h"
#include "runtime/function/render/overlay/origins_overlay.h"
#include "runtime/function/render/overlay/overlay_anti_aliasing.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/overlay/wireframe_overlay.h"

namespace Blunder {

class SlangCompiler;
class VulkanAllocator;
class VulkanContext;

namespace rhi {
class IOffscreenRenderTarget;
}  // namespace rhi

struct ForwardFrameState;

/// Orchestrates all overlay types. Inspired by Blender's overlay::Instance.
///
/// Lifecycle per frame:
///   1. begin_sync() — populates OverlayState, calls begin_sync on all overlays
///   2. draw()       — records Vulkan draw commands for all enabled overlays
///
/// The overlay system draws within the same render pass as the scene, after
/// opaque and transparent passes.
class OverlaySystem final {
 public:
  OverlaySystem() = default;
  ~OverlaySystem();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                  rhi::IOffscreenRenderTarget* offscreen,
                  SlangCompiler* compiler);
  void shutdown();

  /// Sync all overlays with current frame state.
  void begin_sync(const ForwardFrameState& frame_state,
                  uint32_t current_frame);

  /// Draw all enabled overlays. Called within an active render pass.
  void draw(VkCommandBuffer cmd);

  /// Access individual overlays for configuration.
  GridOverlay& grid() { return m_grid; }
  AxesOverlay& axes() { return m_axes; }
  WireframeOverlay& wireframe() { return m_wireframe; }
  OriginsOverlay& origins() { return m_origins; }
  OverlayAntiAliasing& anti_aliasing() { return m_anti_aliasing; }

 private:
  OverlayState m_state;
  OverlayResources m_resources;

  GridOverlay m_grid;
  AxesOverlay m_axes;
  WireframeOverlay m_wireframe;
  OriginsOverlay m_origins;
  OverlayAntiAliasing m_anti_aliasing;
};

}  // namespace Blunder
