#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

namespace Blunder {

class SlangCompiler;
class VulkanContext;

struct VulkanPipelineCreateInfo {
  const char* shader_path{"engine/shaders/basic.slang"};
  bool enable_vertex_input{true};
  VkPrimitiveTopology topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkCullModeFlags cull_mode{VK_CULL_MODE_BACK_BIT};
  bool enable_blend{false};
  bool enable_depth_test{false};
  bool enable_depth_write{false};
  VkCompareOp depth_compare_op{VK_COMPARE_OP_LESS_OR_EQUAL};
  bool enable_depth_bias{false};
  float depth_bias_constant_factor{0.0f};
  float depth_bias_slope_factor{0.0f};
  bool enable_texture_sampling{false};
  VkShaderStageFlags descriptor_stage_flags{VK_SHADER_STAGE_VERTEX_BIT};
};

/// 3D scene pipeline using basic.slang. The pipeline is built against an
/// externally-owned render pass (typically OffscreenRenderTarget's render
/// pass) so the scene can be rendered to an off-screen image instead of
/// directly to the swapchain.
class VulkanPipeline final {
 public:
  VulkanPipeline() = default;
  ~VulkanPipeline() = default;

  void initialize(VulkanContext* context, SlangCompiler* slang_compiler,
                  VkRenderPass render_pass,
                  const VulkanPipelineCreateInfo& create_info = {});
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
  VulkanPipelineCreateInfo m_create_info{};
  VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_graphics_pipeline{VK_NULL_HANDLE};
  VkCommandPool m_command_pool{VK_NULL_HANDLE};
  eastl::vector<VkCommandBuffer> m_command_buffers;
};

}  // namespace Blunder
