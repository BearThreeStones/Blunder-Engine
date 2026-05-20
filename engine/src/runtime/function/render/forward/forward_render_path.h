#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

namespace Blunder {

class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class ShadowMapTarget;
class VulkanTexture;

namespace rhi {
class IOffscreenRenderTarget;
}

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}

struct ForwardFrameState;
struct ForwardOpaqueDraw;

struct ForwardRenderPathInit {
  VulkanContext* vk_context{nullptr};
  VulkanAllocator* vk_allocator{nullptr};
  rhi::IOffscreenRenderTarget* offscreen{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* grid_pipeline{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* opaque_pipeline{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* transparent_pipeline{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* shadow_pipeline{nullptr};
  ShadowMapTarget* shadow_map{nullptr};
  VulkanTexture* fallback_texture{nullptr};
};

/// Single offscreen forward pass: grid + opaque draw list + RHI readback barriers.
class ForwardRenderPath final {
 public:
  static constexpr uint32_t k_max_opaque_draws = 256;

  ForwardRenderPath() = default;
  ~ForwardRenderPath();

  void initialize(const ForwardRenderPathInit& init);
  void shutdown();

  /// Records grid + opaque draws and post-pass copy barriers into `command_buffer`.
  /// Submit, fence wait, and staging map remain the caller's responsibility.
  void renderFrame(VkCommandBuffer command_buffer,
                   const ForwardFrameState& frame_state,
                   const ForwardOpaqueDraw* opaque_draws,
                   uint32_t opaque_draw_count,
                   const ForwardOpaqueDraw* transparent_draws,
                   uint32_t transparent_draw_count,
                   uint32_t frame_index);

 private:
  void bindViewportScissor(VkCommandBuffer cmd, uint32_t width, uint32_t height);
  void drawGrid(VkCommandBuffer cmd, const ForwardFrameState& frame_state,
                uint32_t frame_index);
  void drawOpaqueList(VkCommandBuffer cmd, const ForwardFrameState& frame_state,
                      const ForwardOpaqueDraw* opaque_draws,
                      uint32_t opaque_draw_count, uint32_t frame_index);
  void drawTransparentList(VkCommandBuffer cmd,
                           const ForwardFrameState& frame_state,
                           const ForwardOpaqueDraw* transparent_draws,
                           uint32_t transparent_draw_count, uint32_t frame_index);
  void drawShadowOpaqueList(VkCommandBuffer cmd,
                            const ForwardFrameState& frame_state,
                            const ForwardOpaqueDraw* opaque_draws,
                            uint32_t opaque_draw_count, uint32_t frame_index);

  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};
  rhi::IOffscreenRenderTarget* m_offscreen{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* m_grid_pipeline{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* m_opaque_pipeline{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* m_transparent_pipeline{nullptr};
  vulkan_backend::VulkanGraphicsPipeline* m_shadow_pipeline{nullptr};
  ShadowMapTarget* m_shadow_map{nullptr};
  VulkanTexture* m_fallback_texture{nullptr};

  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_grid_uniform_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_opaque_uniform_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_shadow_uniform_buffers;
  uintptr_t m_grid_descriptor_pool{0};
  eastl::vector<uintptr_t> m_grid_descriptor_sets;
  uintptr_t m_opaque_descriptor_pool{0};
  eastl::vector<uintptr_t> m_opaque_descriptor_sets;
  uintptr_t m_shadow_descriptor_pool{0};
  eastl::vector<uintptr_t> m_shadow_descriptor_sets;
};

}  // namespace Blunder
