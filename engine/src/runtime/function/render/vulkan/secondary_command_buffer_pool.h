#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

#include "runtime/function/render/rhi/rhi_types.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

class VulkanContext;

enum class SecondaryStream : uint8_t {
  viewport = 0,
  camera_preview = 1,
  immediate = 2,
};

enum class SecondaryPass : uint8_t {
  shadow = 0,
  forward_opaque = 1,
  forward_scene_overlay = 2,
  forward_transparent = 3,
  outline_prepass = 4,
  outline_resolve = 5,
  overlay_lines = 6,
  overlay_aa = 7,
  ssao_ao = 8,
  ssao_composite = 9,
  screen_overlay = 10,
  gbuffer_opaque = 11,
  deferred_lighting = 12,
};

class SecondaryCommandBufferPool final {
 public:
  static constexpr uint32_t k_stream_count = 3;
  static constexpr uint32_t k_pass_count = 13;

  static_assert(static_cast<uint32_t>(SecondaryStream::immediate) + 1 ==
                k_stream_count);
  static_assert(static_cast<uint32_t>(SecondaryPass::deferred_lighting) + 1 ==
                k_pass_count);

  SecondaryCommandBufferPool() = default;
  ~SecondaryCommandBufferPool();
  SecondaryCommandBufferPool(const SecondaryCommandBufferPool&) = delete;
  SecondaryCommandBufferPool& operator=(const SecondaryCommandBufferPool&) =
      delete;
  SecondaryCommandBufferPool(SecondaryCommandBufferPool&&) = delete;
  SecondaryCommandBufferPool& operator=(SecondaryCommandBufferPool&&) = delete;

  static uint32_t slotIndex(SecondaryStream stream, SecondaryPass pass,
                            uint32_t frame_index);

  void initialize(VulkanContext* context);
  void shutdown();

  bool isAllocated() const { return m_pool != VK_NULL_HANDLE; }

  VkCommandBuffer begin(SecondaryStream stream, SecondaryPass pass,
                        uint32_t frame_index, VkRenderPass render_pass,
                        VkFramebuffer framebuffer);
  void end(SecondaryStream stream, SecondaryPass pass, uint32_t frame_index);
  VkCommandBuffer get(SecondaryStream stream, SecondaryPass pass,
                      uint32_t frame_index) const;

  void resetFrame(SecondaryStream stream, uint32_t frame_index);

  static void execute(VkCommandBuffer primary, VkCommandBuffer secondary);
  static void execute(VkCommandBuffer primary, const VkCommandBuffer* secondaries,
                      uint32_t count);

  static VkSubpassContents toVkContents(rhi::SubpassContents contents);

 private:
  VulkanContext* m_context{nullptr};
  VkCommandPool m_pool{VK_NULL_HANDLE};
  eastl::vector<VkCommandBuffer> m_buffers;
};

}  // namespace Blunder
