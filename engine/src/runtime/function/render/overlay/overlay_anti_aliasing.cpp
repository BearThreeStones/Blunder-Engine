#include "runtime/function/render/overlay/overlay_anti_aliasing.h"

#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"

namespace Blunder {

OverlayAntiAliasing::~OverlayAntiAliasing() {
  shutdown();
}

void OverlayAntiAliasing::initialize(VulkanContext* ctx,
                                     VulkanAllocator* alloc,
                                     rhi::IOffscreenRenderTarget* /*offscreen*/,
                                     SlangCompiler* /*compiler*/) {
  m_vk_context = ctx;
  m_vk_allocator = alloc;

  // TODO: Create line_tx texture, overlay_tx texture, AA pipeline.
  // The AA shader will be a fullscreen triangle pass that reads:
  //   - depth_tx: scene depth for edge detection
  //   - color_tx: overlay color before AA
  //   - line_tx:  line direction + distance (for subpixel smoothing)
  // and outputs a composited, anti-aliased overlay image.
}

void OverlayAntiAliasing::shutdown() {
  // TODO: Destroy GPU resources
  m_vk_context = nullptr;
  m_vk_allocator = nullptr;
}

void OverlayAntiAliasing::begin_sync(OverlayResources& /*res*/,
                                     const OverlayState& /*state*/) {
  enabled_ = false;  // TODO: enable when AA pipeline is implemented
}

void OverlayAntiAliasing::draw_output(VkCommandBuffer /*cmd*/,
                                      const OverlayState& /*state*/) {
  if (!enabled_) {
    return;
  }
  // TODO: Bind AA pipeline, bind line_tx + overlay_tx descriptors,
  //       draw fullscreen triangle (3 vertices, procedural).
}

}  // namespace Blunder
