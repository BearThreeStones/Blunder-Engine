#include "runtime/function/render/vulkan/bindless_texture_table.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"

namespace Blunder {

uint32_t BindlessTextureIndexTable::acquire(const void* key, bool* inserted) {
  if (inserted != nullptr) {
    *inserted = false;
  }
  if (key == nullptr) {
    return k_fallback_index;
  }
  for (uint32_t i = 1; i < k_capacity; ++i) {
    if (m_keys[i] == key) {
      return i;
    }
  }
  for (uint32_t i = 1; i < k_capacity; ++i) {
    if (m_keys[i] == nullptr) {
      m_keys[i] = key;
      if (inserted != nullptr) {
        *inserted = true;
      }
      return i;
    }
  }
  if (!m_logged_full) {
    m_logged_full = true;
    LOG_WARN(
        "[BindlessTextureTable] table full ({} slots); using fallback index 0",
        k_capacity);
  }
  return k_fallback_index;
}

uint32_t BindlessTextureIndexTable::release(const void* key) {
  if (key == nullptr) {
    return k_capacity;
  }
  for (uint32_t i = 1; i < k_capacity; ++i) {
    if (m_keys[i] == key) {
      m_keys[i] = nullptr;
      return i;
    }
  }
  return k_capacity;
}

void BindlessTextureTable::initialize(VkDevice device) {
  ASSERT(device != VK_NULL_HANDLE);
  shutdown();
  m_device = device;

  VkDescriptorSetLayoutBinding bindings[2]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  bindings[0].descriptorCount = BindlessTextureIndexTable::k_capacity;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  bindings[1].descriptorCount = BindlessTextureIndexTable::k_capacity;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  const VkDescriptorBindingFlags binding_flags[2] = {
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
          VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
          VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
          VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
          VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
  };
  VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{};
  flags_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  flags_info.bindingCount = 2;
  flags_info.pBindingFlags = binding_flags;

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.pNext = &flags_info;
  layout_info.flags =
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  layout_info.bindingCount = 2;
  layout_info.pBindings = bindings;
  const VkResult layout_result =
      vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_set_layout);
  if (layout_result != VK_SUCCESS) {
    LOG_FATAL("[BindlessTextureTable] vkCreateDescriptorSetLayout failed: {}",
              static_cast<int>(layout_result));
  }

  VkDescriptorPoolSize pool_sizes[2]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  pool_sizes[0].descriptorCount = BindlessTextureIndexTable::k_capacity;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  pool_sizes[1].descriptorCount = BindlessTextureIndexTable::k_capacity;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  const VkResult pool_result =
      vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_pool);
  if (pool_result != VK_SUCCESS) {
    LOG_FATAL("[BindlessTextureTable] vkCreateDescriptorPool failed: {}",
              static_cast<int>(pool_result));
  }

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = m_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &m_set_layout;
  const VkResult set_result =
      vkAllocateDescriptorSets(m_device, &alloc_info, &m_set);
  if (set_result != VK_SUCCESS) {
    LOG_FATAL("[BindlessTextureTable] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
}

void BindlessTextureTable::shutdown() {
  if (m_device != VK_NULL_HANDLE && m_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_device, m_pool, nullptr);
  }
  if (m_device != VK_NULL_HANDLE && m_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_device, m_set_layout, nullptr);
  }
  m_pool = VK_NULL_HANDLE;
  m_set_layout = VK_NULL_HANDLE;
  m_set = VK_NULL_HANDLE;
  m_device = VK_NULL_HANDLE;
  m_fallback = nullptr;
  m_indices = BindlessTextureIndexTable{};
}

bool BindlessTextureTable::writeSlot(uint32_t index, VulkanTexture* texture) {
  if (m_set == VK_NULL_HANDLE || texture == nullptr ||
      texture->getImageView() == VK_NULL_HANDLE) {
    return false;
  }

  VkDescriptorImageInfo image_info{};
  image_info.imageView = texture->getImageView();
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo sampler_info{};
  sampler_info.sampler = texture->getSampler();

  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_set;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = index;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[0].descriptorCount = 1;
  writes[0].pImageInfo = &image_info;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = m_set;
  writes[1].dstBinding = 1;
  writes[1].dstArrayElement = index;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[1].descriptorCount = 1;
  writes[1].pImageInfo = &sampler_info;
  vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
  return true;
}

void BindlessTextureTable::setFallback(VulkanTexture* texture) {
  if (texture == nullptr) {
    return;
  }
  // First writer owns slot 0 for the life of that texture. Mesh Preview must
  // not replace the viewport fallback, then destroy it while the viewport
  // still samples index 0.
  if (m_fallback != nullptr && m_fallback != texture) {
    return;
  }
  m_fallback = texture;
  writeSlot(BindlessTextureIndexTable::k_fallback_index, texture);
}

uint32_t BindlessTextureTable::acquire(VulkanTexture* texture) {
  if (texture == nullptr || texture == m_fallback) {
    return BindlessTextureIndexTable::k_fallback_index;
  }
  bool inserted = false;
  const uint32_t index = m_indices.acquire(texture, &inserted);
  if (inserted && !writeSlot(index, texture)) {
    m_indices.release(texture);
    return BindlessTextureIndexTable::k_fallback_index;
  }
  return index;
}

void BindlessTextureTable::release(VulkanTexture* texture) {
  if (texture == nullptr) {
    return;
  }
  if (texture == m_fallback) {
    m_fallback = nullptr;
    return;
  }
  const uint32_t index = m_indices.release(texture);
  if (index < BindlessTextureIndexTable::k_capacity) {
    writeSlot(index, m_fallback);
  }
}

}  // namespace Blunder
