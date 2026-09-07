#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/function/render/vulkan/bindless_texture_table.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"

namespace Blunder {

class Texture2DAsset;
class VulkanAllocator;
class WindowSystem;

struct VulkanContextCreateInfo {
  WindowSystem* window_system{nullptr};
  bool enable_validation{true};
  const char* slang_build_tag{nullptr};
};

class VulkanContext final {
 public:
  VulkanContext() = default;
  ~VulkanContext() = default;

  void initialize(const VulkanContextCreateInfo& info);
  void shutdown();
  VkCommandBuffer beginImmediateCommands();
  void endImmediateCommands(VkCommandBuffer command_buffer);
  /// Submits one-shot immediate commands without blocking; caller owns fence until signaled.
  void submitImmediateCommandsNoWait(VkCommandBuffer command_buffer, VkFence fence);
  void freeImmediateCommandBuffer(VkCommandBuffer command_buffer);

  VkInstance getInstance() const { return m_instance; }
  VkPhysicalDevice getPhysicalDevice() const { return m_physical_device; }
  VkDevice getDevice() const { return m_device; }
  VkQueue getGraphicsQueue() const { return m_graphics_queue; }
  VkQueue getPresentQueue() const { return m_present_queue; }
  uint32_t getGraphicsQueueFamily() const { return m_graphics_queue_family; }
  uint32_t getPresentQueueFamily() const { return m_present_queue_family; }
  VkSurfaceKHR getSurface() const { return m_surface; }
  uint32_t getApiVersion() const { return m_api_version; }
  float getMaxSamplerAnisotropy() const {
    return m_physical_device_properties.limits.maxSamplerAnisotropy;
  }
  bool isSamplerAnisotropyEnabled() const {
    return m_sampler_anisotropy_enabled;
  }

  VkResult createGraphicsPipelines(
      uint32_t create_info_count,
      const VkGraphicsPipelineCreateInfo* create_infos, VkPipeline* pipelines);

  BindlessTextureTable& bindlessTextureTable() { return m_bindless_table; }

  /// One GPU texture per asset identity on this device (viewport + Mesh Preview).
  VulkanTexture* ensureUploadedTexture(VulkanAllocator* allocator,
                                       const Texture2DAsset& asset);
  void destroyUploadedTextures();

  void retireSampledImage(VkImage image, VkImageView view, VkSampler sampler,
                          void* allocation, VulkanAllocator* allocator);
  void onInFlightFenceRetired(uint32_t slot);
  void flushRetiredSampledImages(bool immediate);

 private:
  struct RetiredSampledImage {
    VkImage image{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
    void* allocation{nullptr};
    VulkanAllocator* allocator{nullptr};
    uint32_t required_seq[VulkanSync::k_max_frames_in_flight]{};
  };

  void createInstance();
  void setupDebugMessenger();
  void selectPhysicalDevice();
  void createLogicalDevice();
  void createImmediateCommandPool();
  void createPipelineCache();
  void savePipelineCache();
  void destroyPipelineCache();
  void recreateEmptyPipelineCache();
  void destroyRetiredSampledImage(const RetiredSampledImage& image);

  WindowSystem* m_window_system{nullptr};
  bool m_enable_validation{true};
  bool m_enable_validation_layer{false};
  bool m_sampler_anisotropy_enabled{false};

  VkInstance m_instance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT m_debug_messenger{VK_NULL_HANDLE};
  VkSurfaceKHR m_surface{VK_NULL_HANDLE};
  VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};
  VkPhysicalDeviceProperties m_physical_device_properties{};
  VkDevice m_device{VK_NULL_HANDLE};
  VkQueue m_graphics_queue{VK_NULL_HANDLE};
  VkQueue m_present_queue{VK_NULL_HANDLE};
  VkCommandPool m_immediate_command_pool{VK_NULL_HANDLE};
  uint32_t m_graphics_queue_family{0};
  uint32_t m_present_queue_family{0};
  uint32_t m_api_version{VK_API_VERSION_1_1};
  VkPipelineCache m_pipeline_cache{VK_NULL_HANDLE};
  eastl::string m_slang_build_tag;
  BindlessTextureTable m_bindless_table;
  eastl::unordered_map<eastl::string, eastl::unique_ptr<VulkanTexture>>
      m_uploaded_textures;
  eastl::vector<RetiredSampledImage> m_retired_sampled_images;
  uint32_t m_in_flight_retire_seq[VulkanSync::k_max_frames_in_flight]{};
};

}  // namespace Blunder
