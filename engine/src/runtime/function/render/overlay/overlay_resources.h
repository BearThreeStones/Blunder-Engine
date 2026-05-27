#pragma once

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
};

}  // namespace Blunder
