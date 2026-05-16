#pragma once

#include <vulkan/vulkan.h>

#include "runtime/function/render/rhi/i_command_list.h"

namespace Blunder {

class VulkanContext;

namespace vulkan_backend {

class VulkanCommandList final : public rhi::ICommandList {
 public:
  VulkanCommandList() = default;

  void bind(VulkanContext* context, VkCommandBuffer command_buffer);
  VkCommandBuffer vkCommandBuffer() const { return m_command_buffer; }

  void begin() override;
  void end() override;
  void submit() override;

 private:
  VulkanContext* m_context{nullptr};
  VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
};

}  // namespace vulkan_backend
}  // namespace Blunder
