#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

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

struct OverlayResources;
struct OverlayState;

/// Grid ground-plane overlay with Blender-style anti-moiré mechanisms.
///
/// Owns its own Vulkan pipeline, uniform buffers, and descriptor sets.
/// Extracted from ForwardRenderPath::drawGrid().
class GridOverlay final : public Overlay {
 public:
  GridOverlay() = default;
  ~GridOverlay();

  /// Create the grid pipeline and allocate GPU resources.
  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                  rhi::IOffscreenRenderTarget* offscreen,
                  SlangCompiler* compiler);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_line(VkCommandBuffer cmd, const OverlayState& state) override;

 private:
  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};

  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_pipeline;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_buffers;
  uintptr_t m_descriptor_pool{0};
  eastl::vector<uintptr_t> m_descriptor_sets;
};

}  // namespace Blunder
