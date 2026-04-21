#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

namespace Blunder {

class SlangCompiler;
class VulkanContext;
class VulkanSwapchain;

class VulkanPipeline final {
 public:
  VulkanPipeline() = default;
  ~VulkanPipeline() = default;

  void initialize(VulkanContext* context, VulkanSwapchain* swapchain,
                  SlangCompiler* slang_compiler);
  void shutdown();
  void recreateFramebuffers(VulkanSwapchain* swapchain);

  VkRenderPass getRenderPass() const { return m_render_pass; }
  VkPipelineLayout getPipelineLayout() const { return m_pipeline_layout; }
  VkPipeline getGraphicsPipeline() const { return m_graphics_pipeline; }
  VkCommandPool getCommandPool() const { return m_command_pool; }
  VkCommandBuffer getCommandBuffer(uint32_t frame_index) const {
    return m_command_buffers[frame_index];
  }
  VkFramebuffer getFramebuffer(uint32_t image_index) const {
    return m_framebuffers[image_index];
  }

 private:
  void createRenderPass();
  void createGraphicsPipeline();
  void createFramebuffers();
  void destroyFramebuffers();
  void createCommandPool();
  void createCommandBuffers();

  VulkanContext* m_context{nullptr};
  VulkanSwapchain* m_swapchain{nullptr};
  SlangCompiler* m_slang_compiler{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_graphics_pipeline{VK_NULL_HANDLE};
  eastl::vector<VkFramebuffer> m_framebuffers;
  VkCommandPool m_command_pool{VK_NULL_HANDLE};
  eastl::vector<VkCommandBuffer> m_command_buffers;
};

}  // namespace Blunder
