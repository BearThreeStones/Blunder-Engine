#include "runtime/function/render/vulkan_backend/vulkan_frame_sync.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder::vulkan_backend {

namespace {
constexpr uint64_t k_fence_wait_timeout_ns = 1'000'000'000ULL;
}

void VulkanFrameSync::bind(VulkanSync* sync) { m_sync = sync; }

uint32_t VulkanFrameSync::maxFramesInFlight() const {
  return VulkanSync::k_max_frames_in_flight;
}

void VulkanFrameSync::waitForFrame(uint32_t frame_index) {
  ASSERT(m_sync);
  VkDevice device = m_sync->getInFlightFence(frame_index) != VK_NULL_HANDLE
                        ? nullptr
                        : nullptr;
  (void)device;
  VkFence fence = m_sync->getInFlightFence(frame_index);
  if (fence == VK_NULL_HANDLE) {
    return;
  }
  // Wait is typically paired with reset in RenderSystem; exposed separately.
}

void VulkanFrameSync::resetFrameFence(uint32_t frame_index) {
  ASSERT(m_sync);
  // Device is obtained by callers that have VulkanContext.
  (void)frame_index;
}

void VulkanFrameSync::signalFrameSubmitted(uint32_t frame_index) {
  (void)frame_index;
}

}  // namespace Blunder::vulkan_backend
