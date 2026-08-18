#pragma once

#include <vulkan/vulkan.h>

namespace Blunder {

class VulkanContext;
class VulkanAllocator;
class SlangCompiler;

namespace rhi {
class IOffscreenRenderTarget;
}  // namespace rhi

/// Shared GPU resources for the overlay system.
struct OverlayResources {
  VulkanContext* vk_context{nullptr};
  VulkanAllocator* vk_allocator{nullptr};
  SlangCompiler* slang_compiler{nullptr};
  rhi::IOffscreenRenderTarget* offscreen{nullptr};
  /// Forward-pass render pass (meshes). Depth-tested overlays such as the
  /// ground grid must be created against this pass, not ScreenOverlayPass.
  VkRenderPass scene_render_pass{VK_NULL_HANDLE};
  VkRenderPass screen_render_pass{VK_NULL_HANDLE};
};

}  // namespace Blunder
