#include "runtime/function/render/vulkan/vulkan_pipeline.h"

#include <slang.h>

#include <cstddef>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

void VulkanPipeline::initialize(VulkanContext* context,
                                SlangCompiler* slang_compiler,
                                VkRenderPass render_pass,
                                const VulkanPipelineCreateInfo& create_info) {
  ASSERT(context);
  ASSERT(slang_compiler);
  ASSERT(render_pass != VK_NULL_HANDLE);

  m_context = context;
  m_slang_compiler = slang_compiler;
  m_render_pass = render_pass;
  m_create_info = create_info;

  createDescriptorSetLayout();
  createGraphicsPipeline();
  createCommandPool();
  createCommandBuffers();
}

void VulkanPipeline::shutdown() {
  if (!m_context) {
    return;
  }

  VkDevice device = m_context->getDevice();

  if (m_command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, m_command_pool, nullptr);
    m_command_pool = VK_NULL_HANDLE;
    m_command_buffers.clear();
  }

  if (m_graphics_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_graphics_pipeline, nullptr);
    m_graphics_pipeline = VK_NULL_HANDLE;
  }

  if (m_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);
    m_pipeline_layout = VK_NULL_HANDLE;
  }

  if (m_descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_descriptor_set_layout, nullptr);
    m_descriptor_set_layout = VK_NULL_HANDLE;
  }

  m_render_pass = VK_NULL_HANDLE;
  m_slang_compiler = nullptr;
  m_context = nullptr;
}

void VulkanPipeline::createGraphicsPipeline() {
  ASSERT(m_context);
  ASSERT(m_slang_compiler);
  ASSERT(m_render_pass != VK_NULL_HANDLE);

  VkDevice device = m_context->getDevice();

  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back(
      {"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  eastl::vector<VulkanShader::ShaderStage> shader_stages =
      VulkanShader::loadFromSlang(device, m_slang_compiler,
                                  m_create_info.shader_path, entries);

  eastl::vector<VkPipelineShaderStageCreateInfo> stage_infos;
  stage_infos.reserve(shader_stages.size());
  for (const VulkanShader::ShaderStage& stage : shader_stages) {
    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = stage.stage_flags;
    stage_info.module = stage.module;
    stage_info.pName = stage.entry_point.c_str();
    stage_infos.push_back(stage_info);
  }

  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  VkVertexInputBindingDescription binding_description{};
  eastl::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};
  if (m_create_info.enable_vertex_input) {
    binding_description = Vertex::getBindingDescription();
    attribute_descriptions = Vertex::getAttributeDescriptions();
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();
  }

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = m_create_info.topology;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = m_create_info.cull_mode;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable =
      m_create_info.enable_depth_bias ? VK_TRUE : VK_FALSE;
  rasterizer.depthBiasConstantFactor = m_create_info.depth_bias_constant_factor;
  rasterizer.depthBiasSlopeFactor = m_create_info.depth_bias_slope_factor;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable =
      m_create_info.enable_blend ? VK_TRUE : VK_FALSE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR,
                                     VK_DYNAMIC_STATE_DEPTH_BIAS};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 3;
  dynamic_state.pDynamicStates = dynamic_states;

  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable =
      m_create_info.enable_depth_test ? VK_TRUE : VK_FALSE;
  depth_stencil.depthWriteEnable =
      m_create_info.enable_depth_write ? VK_TRUE : VK_FALSE;
  depth_stencil.depthCompareOp = m_create_info.depth_compare_op;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
  const VkResult layout_result = vkCreatePipelineLayout(
      device, &pipeline_layout_info, nullptr, &m_pipeline_layout);
  if (layout_result != VK_SUCCESS) {
    for (VulkanShader::ShaderStage& stage : shader_stages) {
      VulkanShader::destroyShaderModule(device, &stage.module);
    }
    LOG_FATAL(
        "[VulkanPipeline::createGraphicsPipeline] vkCreatePipelineLayout "
        "failed: {}",
        static_cast<int>(layout_result));
  }

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<uint32_t>(stage_infos.size());
  pipeline_info.pStages = stage_infos.data();
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.pDepthStencilState =
      m_create_info.enable_depth_test ? &depth_stencil : nullptr;
  pipeline_info.layout = m_pipeline_layout;
  pipeline_info.renderPass = m_render_pass;
  pipeline_info.subpass = 0;

  const VkResult pipeline_result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline);

  for (VulkanShader::ShaderStage& stage : shader_stages) {
    VulkanShader::destroyShaderModule(device, &stage.module);
  }

  if (pipeline_result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanPipeline::createGraphicsPipeline] vkCreateGraphicsPipelines "
        "failed: {}",
        static_cast<int>(pipeline_result));
  }
}

void VulkanPipeline::createDescriptorSetLayout() {
  ASSERT(m_context);

  VkDescriptorSetLayoutBinding ubo_layout_binding{};
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;
  ubo_layout_binding.stageFlags = m_create_info.descriptor_stage_flags;
  ubo_layout_binding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 1;
  layout_info.pBindings = &ubo_layout_binding;

  const VkResult result = vkCreateDescriptorSetLayout(
      m_context->getDevice(), &layout_info, nullptr, &m_descriptor_set_layout);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanPipeline::createDescriptorSetLayout] "
        "vkCreateDescriptorSetLayout failed: {}",
        static_cast<int>(result));
  }
}

void VulkanPipeline::createCommandPool() {
  ASSERT(m_context);

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = m_context->getGraphicsQueueFamily();

  const VkResult result = vkCreateCommandPool(
      m_context->getDevice(), &pool_info, nullptr, &m_command_pool);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanPipeline::createCommandPool] vkCreateCommandPool failed: {}",
        static_cast<int>(result));
  }
}

void VulkanPipeline::createCommandBuffers() {
  ASSERT(m_context);
  ASSERT(m_command_pool != VK_NULL_HANDLE);

  m_command_buffers.resize(VulkanSync::k_max_frames_in_flight);

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = m_command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount =
      static_cast<uint32_t>(m_command_buffers.size());

  const VkResult result = vkAllocateCommandBuffers(
      m_context->getDevice(), &alloc_info, m_command_buffers.data());
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanPipeline::createCommandBuffers] vkAllocateCommandBuffers "
        "failed: {}",
        static_cast<int>(result));
  }
}

}  // namespace Blunder
