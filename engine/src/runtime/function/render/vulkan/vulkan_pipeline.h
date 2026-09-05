#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

#include "runtime/function/render/slang/shader_resource_layout.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"

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
  bool enable_skinned_vertex_input{false};
  uint32_t expected_descriptor_bindings[k_max_expected_descriptor_bindings]{0};
  uint32_t expected_descriptor_sets[k_max_expected_descriptor_bindings]{0};
  uint32_t expected_descriptor_binding_count{1};
  uintptr_t shared_descriptor_set_layout{0};
  bool depth_only_subpass{false};
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
  bool usesBindlessTextureTable() const { return m_uses_bindless_texture_table; }
  VkCommandPool getCommandPool() const { return m_command_pool; }
  VkCommandBuffer getCommandBuffer(uint32_t frame_index) const {
    return m_command_buffers[frame_index];
  }

 private:
  void createDescriptorSetLayout(const ShaderResourceLayout& layout);
  void createGraphicsPipeline(
      eastl::vector<VulkanShader::ShaderStage>& shader_stages);
  void createCommandPool();
  void createCommandBuffers();

  VulkanContext* m_context{nullptr};
  SlangCompiler* m_slang_compiler{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  VulkanPipelineCreateInfo m_create_info{};
  bool m_owns_descriptor_set_layout{true};
  bool m_uses_bindless_texture_table{false};
  VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_graphics_pipeline{VK_NULL_HANDLE};
  VkCommandPool m_command_pool{VK_NULL_HANDLE};
  eastl::vector<VkCommandBuffer> m_command_buffers;
};

}  // namespace Blunder
