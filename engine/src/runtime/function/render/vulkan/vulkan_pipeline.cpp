#include "runtime/function/render/vulkan/vulkan_pipeline.h"

#include <slang.h>

#include <cstddef>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_swapchain.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

namespace {

const char* k_shader_path = "shaders/basic.slang";

}  // namespace

void VulkanPipeline::initialize(VulkanContext* context,
                                VulkanSwapchain* swapchain,
                                SlangCompiler* slang_compiler) {
  ASSERT(context);
  ASSERT(swapchain);
  ASSERT(slang_compiler);

  m_context = context;
  m_swapchain = swapchain;
  m_slang_compiler = slang_compiler;

  createRenderPass();
  createGraphicsPipeline();
  createFramebuffers();
  createCommandPool();
  createCommandBuffers();
}

/// <summary>
/// 关闭并清理 Vulkan
/// 管线资源。销毁命令池、命令缓冲区、帧缓冲区、图形管线、管线布局和渲染通道,并释放相关引用
/// </summary>
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

  destroyFramebuffers();

  if (m_graphics_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_graphics_pipeline, nullptr);
    m_graphics_pipeline = VK_NULL_HANDLE;
  }

  if (m_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);
    m_pipeline_layout = VK_NULL_HANDLE;
  }

  if (m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }

  m_slang_compiler = nullptr;
  m_swapchain = nullptr;
  m_context = nullptr;
}

void VulkanPipeline::recreateFramebuffers(VulkanSwapchain* swapchain) {
  ASSERT(m_context);
  ASSERT(swapchain);

  m_swapchain = swapchain;
  destroyFramebuffers();
  createFramebuffers();
}

void VulkanPipeline::createRenderPass() {
  ASSERT(m_context);
  ASSERT(m_swapchain);

  // 定义一个颜色附件，描述交换链图像的格式和使用方式
  VkAttachmentDescription color_attachment{};
  color_attachment.format = m_swapchain->getImageFormat();
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  // 定义一个附件引用，指定子通道将使用哪个附件以及在渲染过程中如何访问它
  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  // 让渲染通道等待 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  // 阶段保证交换链完成从图像的读取，然后才访问它
  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  const VkResult result = vkCreateRenderPass(
      m_context->getDevice(), &render_pass_info, nullptr, &m_render_pass);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanPipeline::createRenderPass] vkCreateRenderPass failed: {}",
        static_cast<int>(result));
  }
}

void VulkanPipeline::createGraphicsPipeline() {
  ASSERT(m_context);
  ASSERT(m_slang_compiler);
  ASSERT(m_render_pass != VK_NULL_HANDLE);

  VkDevice device = m_context->getDevice();

  // 编译具有两个入口点的 .slang 着色器
  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back(
      {"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  eastl::vector<VulkanShader::ShaderStage> shader_stages =
      VulkanShader::loadFromSlang(device, m_slang_compiler, k_shader_path,
                                  entries);

  // 构建 VkPipelineShaderStageCreateInfo 列表以供管线创建使用
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

  VkVertexInputBindingDescription binding_description =
      Vertex::getBindingDescription();
  eastl::array<VkVertexInputAttributeDescription, 2> attribute_descriptions =
      Vertex::getAttributeDescriptions();

  // 设置顶点输入状态：描述将传递给顶点着色器的顶点数据格式
  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = 1;
  vertex_input_info.pVertexBindingDescriptions = &binding_description;
  vertex_input_info.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attribute_descriptions.size());
  vertex_input_info.pVertexAttributeDescriptions =
      attribute_descriptions.data();

  // 设置输入装配状态：将从顶点绘制何种几何图元，以及是否应启用基元重启
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  // 设置光栅化状态：定义如何将几何图元转换为片段
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  // 设置多重采样状态：定义如何执行多重采样（MSAA）
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // 设置颜色混合状态：定义如何将片段着色器输出的颜色与帧缓冲中的现有颜色混合
  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  // 设置动态状态：允许在命令缓冲区录制时更改视口和剪裁矩形
  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic_states;

  // 创建管线布局：描述管线使用的资源（如描述符集和推送常量）
  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
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
  pipeline_info.layout = m_pipeline_layout;
  pipeline_info.renderPass = m_render_pass;
  pipeline_info.subpass = 0;

  const VkResult pipeline_result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline);

  // 清理着色器模块（管线创建后不再需要）
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

/// <summary>
/// 为交换链中的每个图像视图创建 Vulkan 帧缓冲区
/// </summary>
void VulkanPipeline::createFramebuffers() {
  ASSERT(m_context);
  ASSERT(m_swapchain);
  ASSERT(m_render_pass != VK_NULL_HANDLE);

  const eastl::vector<VkImageView>& image_views = m_swapchain->getImageViews();
  m_framebuffers.resize(image_views.size());

  VkExtent2D extent = m_swapchain->getExtent();
  for (size_t i = 0; i < image_views.size(); ++i) {
    VkImageView attachments[] = {image_views[i]};

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = m_render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = extent.width;
    framebuffer_info.height = extent.height;
    framebuffer_info.layers = 1;

    const VkResult result = vkCreateFramebuffer(
        m_context->getDevice(), &framebuffer_info, nullptr, &m_framebuffers[i]);
    if (result != VK_SUCCESS) {
      LOG_FATAL(
          "[VulkanPipeline::createFramebuffers] vkCreateFramebuffer failed: {}",
          static_cast<int>(result));
    }
  }
}

/// <summary>
/// 销毁所有帧缓冲区对象
/// </summary>
void VulkanPipeline::destroyFramebuffers() {
  if (!m_context) {
    return;
  }

  for (VkFramebuffer framebuffer : m_framebuffers) {
    if (framebuffer != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(m_context->getDevice(), framebuffer, nullptr);
    }
  }
  m_framebuffers.clear();
}

/// <summary>
/// 创建 Vulkan 命令池
/// </summary>
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

/// <summary>
/// 创建 Vulkan 命令缓冲区
/// </summary>
void VulkanPipeline::createCommandBuffers() {
  ASSERT(m_context);
  ASSERT(m_command_pool != VK_NULL_HANDLE);

  m_command_buffers.resize(VulkanSync::k_max_frames_in_flight);

  // 分配命令缓冲区
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
