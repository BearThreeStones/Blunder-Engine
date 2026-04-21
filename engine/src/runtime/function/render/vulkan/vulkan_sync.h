#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

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

  VkSemaphore getImageAvailableSemaphore(uint32_t frame_index) const {
    return m_image_available_semaphores[frame_index];
  }

  VkSemaphore getRenderFinishedSemaphore(uint32_t image_index) const {
    return m_render_finished_semaphores[image_index];
  }

  VkFence getInFlightFence(uint32_t frame_index) const {
    return m_in_flight_fences[frame_index];
  }

  // 追踪每个交换链图像正在被哪个 fence 使用，以避免在使用时重用
  VkFence getImageInFlightFence(uint32_t image_index) const;
  void setImageInFlightFence(uint32_t image_index, VkFence fence);

 private:
  VulkanContext* m_context{nullptr};
  eastl::array<VkSemaphore, k_max_frames_in_flight>
      m_image_available_semaphores{VK_NULL_HANDLE, VK_NULL_HANDLE};
  eastl::vector<VkSemaphore> m_render_finished_semaphores;
  eastl::array<VkFence, k_max_frames_in_flight> m_in_flight_fences{
      VK_NULL_HANDLE, VK_NULL_HANDLE};
  // 每个交换链图像一个 fence，用于跟踪正在使用的情况
  eastl::vector<VkFence> m_images_in_flight;
};

}  // namespace Blunder
