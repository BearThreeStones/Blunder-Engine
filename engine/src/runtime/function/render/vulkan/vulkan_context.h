#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace Blunder {

class WindowSystem;

struct VulkanContextCreateInfo {
  WindowSystem* window_system{nullptr};
  bool enable_validation{true};
};

class VulkanContext final {
 public:
  VulkanContext() = default;
  ~VulkanContext() = default;

  void initialize(const VulkanContextCreateInfo& info);
  void shutdown();

  VkInstance getInstance() const { return m_instance; }
  VkPhysicalDevice getPhysicalDevice() const { return m_physical_device; }
  VkDevice getDevice() const { return m_device; }
  VkQueue getGraphicsQueue() const { return m_graphics_queue; }
  VkQueue getPresentQueue() const { return m_present_queue; }
  uint32_t getGraphicsQueueFamily() const { return m_graphics_queue_family; }
  uint32_t getPresentQueueFamily() const { return m_present_queue_family; }
  VkSurfaceKHR getSurface() const { return m_surface; }
  uint32_t getApiVersion() const { return m_api_version; }

 private:
  void createInstance();
  void setupDebugMessenger();
  void selectPhysicalDevice();
  void createLogicalDevice();

  WindowSystem* m_window_system{nullptr};
  bool m_enable_validation{true};
  bool m_enable_validation_layer{false};

  VkInstance m_instance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT m_debug_messenger{VK_NULL_HANDLE};
  VkSurfaceKHR m_surface{VK_NULL_HANDLE};
  VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};
  VkDevice m_device{VK_NULL_HANDLE};
  VkQueue m_graphics_queue{VK_NULL_HANDLE};
  VkQueue m_present_queue{VK_NULL_HANDLE};
  uint32_t m_graphics_queue_family{0};
  uint32_t m_present_queue_family{0};
  uint32_t m_api_version{VK_API_VERSION_1_1};
};

}  // namespace Blunder
