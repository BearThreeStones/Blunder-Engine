#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

namespace Blunder {

class SlangCompiler;
class VulkanContext;

/// 3D scene pipeline using basic.slang. The pipeline is built against an
/// externally-owned render pass (typically OffscreenRenderTarget's render
/// pass) so the scene can be rendered to an off-screen image instead of
/// directly to the swapchain.
class VulkanPipeline final {
 public:
  VulkanPipeline() = default;
  ~VulkanPipeline() = default;

  void initialize(VulkanContext* context, SlangCompiler* slang_compiler,
                  VkRenderPass render_pass);
  void shutdown();

  VkPipelineLayout getPipelineLayout() const { return m_pipeline_layout; }
  VkPipeline getGraphicsPipeline() const { return m_graphics_pipeline; }
  VkDescriptorSetLayout getDescriptorSetLayout() const {
    return m_descriptor_set_layout;
  }
  VkCommandPool getCommandPool() const { return m_command_pool; }
  VkCommandBuffer getCommandBuffer(uint32_t frame_index) const {
    return m_command_buffers[frame_index];
  }

 private:
  void createDescriptorSetLayout();
  void createGraphicsPipeline();
  void createCommandPool();
  void createCommandBuffers();

  VulkanContext* m_context{nullptr};
  SlangCompiler* m_slang_compiler{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_graphics_pipeline{VK_NULL_HANDLE};
  VkCommandPool m_command_pool{VK_NULL_HANDLE};
  eastl::vector<VkCommandBuffer> m_command_buffers;
};

}  // namespace Blunder
