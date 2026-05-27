#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;

namespace rhi {
class IOffscreenRenderTarget;
}  // namespace rhi

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}  // namespace vulkan_backend

/// Post-process anti-aliasing pass for overlay line rendering.
///
/// Similar to Blender's overlay AntiAliasing:
/// - Reads a line direction/distance texture (line_tx)
/// - Reads the overlay color texture (overlay_tx)
/// - Composites AA-smoothed lines onto the final output
///
/// TODO: Implement fullscreen AA pass with line_tx MRT.
class OverlayAntiAliasing final : public Overlay {
 public:
  OverlayAntiAliasing() = default;
  ~OverlayAntiAliasing();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                  rhi::IOffscreenRenderTarget* offscreen,
                  SlangCompiler* compiler);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;

  /// The AA pass draws during the output phase (after all overlays).
  void draw_output(VkCommandBuffer cmd, const OverlayState& state);

 private:
  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};

  // TODO: AA pipeline, line_tx texture, overlay_tx texture, descriptors
  // eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_pipeline;
};

}  // namespace Blunder
