#include "runtime/function/render/overlay/overlay_anti_aliasing.h"

#include <slang.h>

#include <glm/vec4.hpp>
#include <vk_mem_alloc.h>

#include "EASTL/unique_ptr.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/overlay/overlay_line_targets.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"

namespace Blunder {

namespace {

struct OverlayAaUniformData {
  glm::vec4 screen_params;
};

VkPipeline createAaPipeline(VkDevice device, VkRenderPass render_pass,
                              VkPipelineLayout pipeline_layout,
                              const eastl::vector<VulkanShader::ShaderStage>& stages) {
  eastl::vector<VkPipelineShaderStageCreateInfo> stage_infos;
  for (const VulkanShader::ShaderStage& stage : stages) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage.stage_flags;
    info.module = stage.module;
    info.pName = stage.entry_point.c_str();
    stage_infos.push_back(info);
  }

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blend{};
  blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &blend;

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic_states;

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<uint32_t>(stage_infos.size());
  pipeline_info.pStages = stage_infos.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OverlayAntiAliasing] vkCreateGraphicsPipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

}  // namespace

OverlayAntiAliasing::~OverlayAntiAliasing() {
  shutdown();
}

void OverlayAntiAliasing::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                                     rhi::IOffscreenRenderTarget* /*offscreen*/,
                                     SlangCompiler* compiler,
                                     OverlayLineTargets* line_targets) {
  ASSERT(ctx);
  ASSERT(alloc);
  ASSERT(compiler);
  ASSERT(line_targets);
  m_context = ctx;
  m_allocator = alloc;
  m_compiler = compiler;
  m_line_targets = line_targets;

  VkDevice device = ctx->getDevice();

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(device, &sampler_info, nullptr, &m_linear_sampler);

  m_uniform_buffer = eastl::make_unique<VulkanBuffer>();
  m_uniform_buffer->create(alloc, sizeof(OverlayAaUniformData),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU);

  createRenderPass();
  createDescriptorResources();
  createPipeline();

  const VkExtent2D extent = line_targets->extent();
  m_width = extent.width;
  m_height = extent.height;
  createSceneSnapshotResources();
}

void OverlayAntiAliasing::shutdown() {
  destroyPipeline();
  destroyDescriptorResources();
  destroySceneSnapshotResources();
  destroyRenderPass();

  if (m_context && m_linear_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_context->getDevice(), m_linear_sampler, nullptr);
    m_linear_sampler = VK_NULL_HANDLE;
  }
  if (m_uniform_buffer) {
    m_uniform_buffer->destroy();
    m_uniform_buffer.reset();
  }

  m_line_targets = nullptr;
  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
}

void OverlayAntiAliasing::resize(uint32_t width, uint32_t height) {
  if (width == m_width && height == m_height) {
    return;
  }
  m_width = width;
  m_height = height;
  destroySceneSnapshotResources();
  createSceneSnapshotResources();
}

void OverlayAntiAliasing::begin_sync(OverlayResources& /*res*/,
                                     const OverlayState& /*state*/) {
  enabled_ = true;
}

void OverlayAntiAliasing::createRenderPass() {
  VkAttachmentDescription color{};
  color.format = VK_FORMAT_R8G8B8A8_UNORM;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo rp_info{};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.attachmentCount = 1;
  rp_info.pAttachments = &color;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;
  rp_info.dependencyCount = 1;
  rp_info.pDependencies = &dep;

  vkCreateRenderPass(m_context->getDevice(), &rp_info, nullptr, &m_render_pass);
}

void OverlayAntiAliasing::destroyRenderPass() {
  if (m_context && m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_context->getDevice(), m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }
}

void OverlayAntiAliasing::createDescriptorResources() {
  VkDevice device = m_context->getDevice();

  VkDescriptorSetLayoutBinding bindings[7]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  for (uint32_t i = 1; i <= 6; ++i) {
    bindings[i].binding = i;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[i].descriptorType =
        (i % 2 == 1) ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                     : VK_DESCRIPTOR_TYPE_SAMPLER;
  }

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 7;
  layout_info.pBindings = bindings;
  vkCreateDescriptorSetLayout(device, &layout_info, nullptr,
                              &m_descriptor_layout);

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &m_descriptor_layout;
  vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                         &m_pipeline_layout);

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 3},
  };
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 3;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool);

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = m_descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &m_descriptor_layout;
  vkAllocateDescriptorSets(device, &alloc_info, &m_descriptor_set);
}

void OverlayAntiAliasing::destroyDescriptorResources() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  m_descriptor_set = VK_NULL_HANDLE;
  if (m_descriptor_pool) {
    vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
  }
  if (m_pipeline_layout) {
    vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);
    m_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_descriptor_layout) {
    vkDestroyDescriptorSetLayout(device, m_descriptor_layout, nullptr);
    m_descriptor_layout = VK_NULL_HANDLE;
  }
}

void OverlayAntiAliasing::createSceneSnapshotResources() {
  if (!m_context || !m_allocator || m_width == 0 || m_height == 0) {
    return;
  }

  VkDevice device = m_context->getDevice();

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.extent = {m_width, m_height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  const VkResult result = vmaCreateImage(
      m_allocator->getAllocator(), &image_info, &alloc_info,
      &m_scene_snapshot_image, &m_scene_snapshot_allocation, nullptr);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OverlayAntiAliasing] vmaCreateImage scene snapshot failed: {}",
              static_cast<int>(result));
  }

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_scene_snapshot_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;
  vkCreateImageView(device, &view_info, nullptr, &m_scene_snapshot_view);
  m_scene_snapshot_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void OverlayAntiAliasing::destroySceneSnapshotResources() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_scene_snapshot_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_scene_snapshot_view, nullptr);
    m_scene_snapshot_view = VK_NULL_HANDLE;
  }
  if (m_scene_snapshot_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), m_scene_snapshot_image,
                    m_scene_snapshot_allocation);
    m_scene_snapshot_image = VK_NULL_HANDLE;
    m_scene_snapshot_allocation = VK_NULL_HANDLE;
  }
  m_scene_snapshot_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void OverlayAntiAliasing::copySceneSnapshot(VkCommandBuffer cmd,
                                            OffscreenRenderTarget* offscreen) {
  if (!offscreen || m_scene_snapshot_image == VK_NULL_HANDLE) {
    return;
  }

  VkImage src = offscreen->getImage();
  VkImage dst = m_scene_snapshot_image;

  VkImageMemoryBarrier barriers[2]{};
  barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].image = src;
  barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barriers[0].subresourceRange.levelCount = 1;
  barriers[0].subresourceRange.layerCount = 1;
  barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

  barriers[1] = barriers[0];
  barriers[1].image = dst;
  barriers[1].oldLayout = m_scene_snapshot_layout;
  barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barriers[1].srcAccessMask = 0;
  barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 2, barriers);

  VkImageCopy copy_region{};
  copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.srcSubresource.layerCount = 1;
  copy_region.dstSubresource = copy_region.srcSubresource;
  copy_region.extent = {m_width, m_height, 1};
  vkCmdCopyImage(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  VkImageMemoryBarrier post_barriers[2]{};
  post_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  post_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  post_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  post_barriers[0].image = src;
  post_barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  post_barriers[0].subresourceRange.levelCount = 1;
  post_barriers[0].subresourceRange.layerCount = 1;
  post_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  post_barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  post_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  post_barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  post_barriers[1] = post_barriers[0];
  post_barriers[1].image = dst;
  post_barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  post_barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  post_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  post_barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 2, post_barriers);

  m_scene_snapshot_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void OverlayAntiAliasing::writeDescriptors() {
  if (!m_line_targets || m_scene_snapshot_view == VK_NULL_HANDLE) {
    return;
  }
  VkDevice device = m_context->getDevice();

  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = m_uniform_buffer->getBuffer();
  ubo_info.range = sizeof(OverlayAaUniformData);

  VkDescriptorImageInfo scene_image{};
  scene_image.imageView = m_scene_snapshot_view;
  scene_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo overlay_image{};
  overlay_image.imageView = m_line_targets->overlayColorView();
  overlay_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo line_image{};
  line_image.imageView = m_line_targets->lineDataView();
  line_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo sampler_info{};
  sampler_info.sampler = m_linear_sampler;

  VkWriteDescriptorSet writes[7]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_descriptor_set;
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubo_info;

  VkDescriptorImageInfo* images[] = {&scene_image, &overlay_image, &line_image};
  for (uint32_t i = 0; i < 3; ++i) {
    writes[i * 2 + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i * 2 + 1].dstSet = m_descriptor_set;
    writes[i * 2 + 1].dstBinding = i * 2 + 1;
    writes[i * 2 + 1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[i * 2 + 1].descriptorCount = 1;
    writes[i * 2 + 1].pImageInfo = images[i];

    writes[i * 2 + 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i * 2 + 2].dstSet = m_descriptor_set;
    writes[i * 2 + 2].dstBinding = i * 2 + 2;
    writes[i * 2 + 2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[i * 2 + 2].descriptorCount = 1;
    writes[i * 2 + 2].pImageInfo = &sampler_info;
  }
  vkUpdateDescriptorSets(device, 7, writes, 0, nullptr);
}

void OverlayAntiAliasing::createPipeline() {
  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back(
      {"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  auto stages = VulkanShader::loadFromSlang(
      m_context->getDevice(), m_compiler, "engine/shaders/overlay_aa.slang",
      entries);
  m_pipeline = createAaPipeline(m_context->getDevice(), m_render_pass,
                                m_pipeline_layout, stages);
  for (auto& stage : stages) {
    VulkanShader::destroyShaderModule(m_context->getDevice(), &stage.module);
  }
}

void OverlayAntiAliasing::destroyPipeline() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_pipeline) {
    vkDestroyPipeline(device, m_pipeline, nullptr);
    m_pipeline = VK_NULL_HANDLE;
  }
}

void OverlayAntiAliasing::apply(VkCommandBuffer cmd,
                                OffscreenRenderTarget* offscreen,
                                const OverlayState& state) {
  if (!enabled_ || !offscreen || m_pipeline == VK_NULL_HANDLE ||
      m_width == 0 || m_height == 0) {
    return;
  }

  offscreen->cmdBarrierToShaderRead(cmd);
  copySceneSnapshot(cmd, offscreen);
  writeDescriptors();

  OverlayAaUniformData ubo{};
  ubo.screen_params =
      glm::vec4(1.0f / static_cast<float>(m_width),
                1.0f / static_cast<float>(m_height), 0.0f, 0.0f);
  m_uniform_buffer->upload(&ubo, sizeof(ubo));

  VkViewport viewport{};
  viewport.width = static_cast<float>(state.viewport_width);
  viewport.height = static_cast<float>(state.viewport_height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0},
                   {state.viewport_width, state.viewport_height}};

  VkClearValue clear{};
  clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_render_pass;
  begin.framebuffer = offscreen->getFramebuffer();
  begin.renderArea.extent = {m_width, m_height};
  begin.clearValueCount = 1;
  begin.pClearValues = &clear;
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline_layout, 0, 1, &m_descriptor_set, 0,
                          nullptr);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);

  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

}  // namespace Blunder
