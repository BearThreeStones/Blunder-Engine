#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace Blunder {

class VulkanTexture;

/// Host-side stable indices for the Bindless texture table. Slot 0 is fallback
/// and is never assigned to a loaded texture.
class BindlessTextureIndexTable {
 public:
  static constexpr uint32_t k_capacity = 1024;
  static constexpr uint32_t k_fallback_index = 0;

  uint32_t acquire(const void* key, bool* inserted = nullptr);
  uint32_t release(const void* key);

 private:
  const void* m_keys[k_capacity]{};
  bool m_logged_full{false};
};

/// One sampled-image + sampler table per Vulkan device.
class BindlessTextureTable {
 public:
  void initialize(VkDevice device);
  void shutdown();

  void setFallback(VulkanTexture* texture);
  uint32_t acquire(VulkanTexture* texture);
  void release(VulkanTexture* texture);
  VulkanTexture* fallback() const { return m_fallback; }

  VkDescriptorSetLayout descriptorSetLayout() const {
    return m_set_layout;
  }
  VkDescriptorSet descriptorSet() const { return m_set; }

 private:
  bool writeSlot(uint32_t index, VulkanTexture* texture);

  BindlessTextureIndexTable m_indices;
  VkDevice m_device{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_set_layout{VK_NULL_HANDLE};
  VkDescriptorPool m_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_set{VK_NULL_HANDLE};
  VulkanTexture* m_fallback{nullptr};
};

}  // namespace Blunder
