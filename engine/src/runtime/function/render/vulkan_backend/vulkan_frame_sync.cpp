#include "runtime/function/render/vulkan_backend/vulkan_frame_sync.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder::vulkan_backend {

void VulkanFrameSync::bind(VulkanSync* sync) { m_sync = sync; }

uint32_t VulkanFrameSync::maxFramesInFlight() const {
  return VulkanSync::k_max_frames_in_flight;
}

void VulkanFrameSync::waitForFrame(uint32_t frame_index) {
  ASSERT(m_sync);
  m_sync->waitSlot(frame_index, UINT64_MAX);
}

void VulkanFrameSync::resetFrameFence(uint32_t frame_index) {
  (void)frame_index;
}

void VulkanFrameSync::signalFrameSubmitted(uint32_t frame_index) {
  (void)frame_index;
}

}  // namespace Blunder::vulkan_backend
