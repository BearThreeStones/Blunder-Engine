#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/array.h"
#include "EASTL/vector.h"

namespace Blunder {

class VulkanContext;

class VulkanSync final {
 public:
  static constexpr uint32_t k_max_frames_in_flight = 2;

  VulkanSync() = default;
  ~VulkanSync() = default;

  void initialize(VulkanContext* context, uint32_t swapchain_image_count);
  void shutdown();
  void recreateRenderFinishedSemaphores(uint32_t swapchain_image_count);

  bool isTimelineAllocated() const { return m_timeline != VK_NULL_HANDLE; }
  VkSemaphore timelineSemaphore() const { return m_timeline; }

  uint64_t issueValue();
  uint64_t queueSubmit(VkQueue queue, const VkSubmitInfo& base_info);

  void setSlotValue(uint32_t slot, uint64_t value);
  uint64_t slotValue(uint32_t slot) const;
  bool slotReached(uint32_t slot) const;
  bool valueReached(uint64_t value) const;
  VkResult waitValue(uint64_t value, uint64_t timeout_ns) const;
  VkResult waitSlot(uint32_t slot, uint64_t timeout_ns) const;

  VkSemaphore getImageAvailableSemaphore(uint32_t frame_index) const {
    return m_image_available_semaphores[frame_index];
  }

  VkSemaphore getRenderFinishedSemaphore(uint32_t image_index) const {
    return m_render_finished_semaphores[image_index];
  }

  VkFence getImageInFlightFence(uint32_t image_index) const;
  void setImageInFlightFence(uint32_t image_index, VkFence fence);

 private:
  VulkanContext* m_context{nullptr};
  VkSemaphore m_timeline{VK_NULL_HANDLE};
  uint64_t m_next_timeline_value{0};
  eastl::array<uint64_t, k_max_frames_in_flight> m_slot_values{0, 0};
  eastl::array<VkSemaphore, k_max_frames_in_flight>
      m_image_available_semaphores{VK_NULL_HANDLE, VK_NULL_HANDLE};
  eastl::vector<VkSemaphore> m_render_finished_semaphores;
  eastl::vector<VkFence> m_images_in_flight;
};

}  // namespace Blunder
