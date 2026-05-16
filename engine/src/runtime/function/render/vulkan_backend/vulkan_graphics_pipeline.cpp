#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"

#include <vulkan/vulkan.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"

namespace Blunder::vulkan_backend {

namespace {

VulkanPipelineCreateInfo toVulkanPipelineCreateInfo(
    const rhi::GraphicsPipelineDesc& desc) {
  VulkanPipelineCreateInfo info{};
  info.shader_path = desc.shader_path;
  info.enable_vertex_input = desc.enable_vertex_input;
  info.topology = desc.topology == rhi::PrimitiveTopology::LineList
                      ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                      : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  info.cull_mode = desc.cull_mode == rhi::CullMode::None ? VK_CULL_MODE_NONE
                                                         : VK_CULL_MODE_BACK_BIT;
  info.enable_blend = desc.enable_blend;
  info.enable_depth_test = desc.enable_depth_test;
  info.enable_depth_write = desc.enable_depth_write;
  info.enable_depth_bias = desc.enable_depth_bias;
  info.depth_bias_constant_factor = desc.depth_bias_constant_factor;
  info.depth_bias_slope_factor = desc.depth_bias_slope_factor;
  info.enable_texture_sampling = desc.enable_texture_sampling;
  info.descriptor_stage_flags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  return info;
}

}  // namespace

VulkanGraphicsPipeline::~VulkanGraphicsPipeline() { shutdown(); }

void VulkanGraphicsPipeline::bind(VulkanContext* context,
                                  SlangCompiler* compiler) {
  m_context = context;
  m_compiler = compiler;
}

void VulkanGraphicsPipeline::initialize(
    rhi::IOffscreenRenderTarget& render_target,
    const rhi::GraphicsPipelineDesc& desc) {
  auto& vk_target = static_cast<VulkanOffscreenTarget&>(render_target);
  ASSERT(m_context && m_compiler && vk_target.nativeTarget());

  m_pipeline = eastl::make_shared<VulkanPipeline>();
  m_pipeline->initialize(m_context, m_compiler, vk_target.nativeTarget()->getRenderPass(),
                         toVulkanPipelineCreateInfo(desc));

  for (uint32_t i = 0; i < k_max_command_lists; ++i) {
    m_command_lists[i] = eastl::make_unique<VulkanCommandList>();
    m_command_lists[i]->bind(m_context, m_pipeline->getCommandBuffer(i));
  }
}

void VulkanGraphicsPipeline::shutdown() {
  if (m_pipeline) {
    m_pipeline->shutdown();
    m_pipeline.reset();
  }
  for (eastl::unique_ptr<VulkanCommandList>& list : m_command_lists) {
    list.reset();
  }
}

rhi::ICommandList* VulkanGraphicsPipeline::commandList(uint32_t frame_index) {
  return m_command_lists[frame_index % k_max_command_lists].get();
}

const rhi::ICommandList* VulkanGraphicsPipeline::commandList(
    uint32_t frame_index) const {
  return m_command_lists[frame_index % k_max_command_lists].get();
}

}  // namespace Blunder::vulkan_backend
