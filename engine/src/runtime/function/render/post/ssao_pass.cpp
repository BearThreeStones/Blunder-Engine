#include "runtime/function/render/post/ssao_pass.h"

#include <cmath>
#include <cstdint>

#include <slang.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <vk_mem_alloc.h>

#include "EASTL/unique_ptr.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

namespace {

struct SsaOUniformData {
  glm::mat4 inv_projection;
  glm::mat4 projection;
  glm::vec4 clip_params;
  glm::vec4 screen_params;
  glm::vec4 effect_params;
};

constexpr uint32_t k_noise_size = 4;

eastl::vector<uint8_t> buildNoisePixels() {
  eastl::vector<uint8_t> pixels(static_cast<size_t>(k_noise_size) * k_noise_size * 4u);
  uint32_t seed = 2166136261u;
  for (uint32_t i = 0; i < k_noise_size * k_noise_size; ++i) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    const float angle =
        static_cast<float>(seed & 0xFFFFu) / 65535.0f * 6.2831853f;
    const size_t index = static_cast<size_t>(i) * 4u;
    pixels[index + 0] = static_cast<uint8_t>((std::cos(angle) * 0.5f + 0.5f) * 255.0f);
    pixels[index + 1] = static_cast<uint8_t>((std::sin(angle) * 0.5f + 0.5f) * 255.0f);
    pixels[index + 2] = static_cast<uint8_t>((seed & 0xFFu));
    pixels[index + 3] = 255u;
  }
  return pixels;
}

VkPipeline createFullscreenPipeline(
    VkDevice device, VkRenderPass render_pass,
    VkPipelineLayout pipeline_layout,
    const eastl::vector<VulkanShader::ShaderStage>& stages) {
  eastl::vector<VkPipelineShaderStageCreateInfo> stage_infos;
  stage_infos.reserve(stages.size());
  for (const VulkanShader::ShaderStage& stage : stages) {
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

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &blend_attachment;

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
  pipeline_info.pVertexInputState = &vertex_input_info;
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
    LOG_FATAL("[SsaOPass] vkCreateGraphicsPipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

}  // namespace

SsaOPass::~SsaOPass() { shutdown(); }

void SsaOPass::initialize(VulkanContext* context, VulkanAllocator* allocator,
                          SlangCompiler* compiler) {
  ASSERT(context);
  ASSERT(allocator);
  ASSERT(compiler);

  m_context = context;
  m_allocator = allocator;
  m_compiler = compiler;

  VkDevice device = m_context->getDevice();

  VkSamplerCreateInfo depth_sampler_info{};
  depth_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  depth_sampler_info.magFilter = VK_FILTER_NEAREST;
  depth_sampler_info.minFilter = VK_FILTER_NEAREST;
  depth_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depth_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  depth_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(device, &depth_sampler_info, nullptr, &m_depth_sampler);

  VkSamplerCreateInfo linear_sampler_info = depth_sampler_info;
  linear_sampler_info.magFilter = VK_FILTER_LINEAR;
  linear_sampler_info.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(device, &linear_sampler_info, nullptr, &m_linear_sampler);

  m_uniform_buffer = eastl::make_unique<VulkanBuffer>();
  m_uniform_buffer->create(m_allocator, sizeof(SsaOUniformData),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU);

  createNoiseTexture();
  createRenderPasses();
  createDescriptorResources();
  createPipelines();
}

void SsaOPass::shutdown() {
  if (!m_context) {
    return;
  }

  destroyPipelines();
  destroyDescriptorResources();
  destroyRenderPasses();
  destroyAoResources();

  VkDevice device = m_context->getDevice();
  if (m_linear_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_linear_sampler, nullptr);
    m_linear_sampler = VK_NULL_HANDLE;
  }
  if (m_depth_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_depth_sampler, nullptr);
    m_depth_sampler = VK_NULL_HANDLE;
  }

  if (m_uniform_buffer) {
    m_uniform_buffer->destroy();
    m_uniform_buffer.reset();
  }

  m_noise_texture.reset();
  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
  m_width = 0;
  m_height = 0;
}

void SsaOPass::resize(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return;
  }
  if (width == m_width && height == m_height) {
    return;
  }
  m_width = width;
  m_height = height;

  destroyAoResources();
  if (m_composite_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(m_context->getDevice(), m_composite_framebuffer,
                         nullptr);
    m_composite_framebuffer = VK_NULL_HANDLE;
  }
  createAoResources();
}

void SsaOPass::createNoiseTexture() {
  Asset::Meta meta;
  meta.virtual_path = "generated://render/ssao_noise";
  Texture2DAsset noise_asset(meta, k_noise_size, k_noise_size, 4u,
                             buildNoisePixels());
  m_noise_texture = eastl::make_unique<VulkanTexture>();
  m_noise_texture->createFromTexture2DAsset(m_context, m_allocator, noise_asset);
}

void SsaOPass::createRenderPasses() {
  VkDevice device = m_context->getDevice();

  {
    VkAttachmentDescription ao_color{};
    ao_color.format = VK_FORMAT_R8_UNORM;
    ao_color.samples = VK_SAMPLE_COUNT_1_BIT;
    ao_color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ao_color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ao_color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ao_color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference ao_ref{};
    ao_ref.attachment = 0;
    ao_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ao_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &ao_color;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies = &dependency;

    const VkResult result =
        vkCreateRenderPass(device, &rp_info, nullptr, &m_ao_render_pass);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[SsaOPass] AO vkCreateRenderPass failed: {}",
                static_cast<int>(result));
    }
  }

  {
    VkAttachmentDescription scene_color{};
    scene_color.format = VK_FORMAT_R8G8B8A8_UNORM;
    scene_color.samples = VK_SAMPLE_COUNT_1_BIT;
    scene_color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    scene_color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    scene_color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scene_color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference scene_ref{};
    scene_ref.attachment = 0;
    scene_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &scene_ref;

    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                    VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &scene_color;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 2;
    rp_info.pDependencies = dependencies;

    const VkResult result =
        vkCreateRenderPass(device, &rp_info, nullptr, &m_composite_render_pass);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[SsaOPass] composite vkCreateRenderPass failed: {}",
                static_cast<int>(result));
    }
  }
}

void SsaOPass::destroyRenderPasses() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_composite_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_composite_render_pass, nullptr);
    m_composite_render_pass = VK_NULL_HANDLE;
  }
  if (m_ao_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_ao_render_pass, nullptr);
    m_ao_render_pass = VK_NULL_HANDLE;
  }
}

void SsaOPass::createAoResources() {
  if (m_width == 0 || m_height == 0) {
    return;
  }
  ASSERT(m_ao_render_pass != VK_NULL_HANDLE);

  VkDevice device = m_context->getDevice();

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_R8_UNORM;
  image_info.extent = {m_width, m_height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  const VkResult img_result = vmaCreateImage(
      m_allocator->getAllocator(), &image_info, &alloc_info, &m_ao_image,
      &m_ao_allocation, nullptr);
  if (img_result != VK_SUCCESS) {
    LOG_FATAL("[SsaOPass] vmaCreateImage AO failed: {}", static_cast<int>(img_result));
  }

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_ao_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = VK_FORMAT_R8_UNORM;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;
  vkCreateImageView(device, &view_info, nullptr, &m_ao_image_view);

  VkImageView attachments[] = {m_ao_image_view};
  VkFramebufferCreateInfo fb_info{};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.renderPass = m_ao_render_pass;
  fb_info.attachmentCount = 1;
  fb_info.pAttachments = attachments;
  fb_info.width = m_width;
  fb_info.height = m_height;
  fb_info.layers = 1;
  vkCreateFramebuffer(device, &fb_info, nullptr, &m_ao_framebuffer);

  m_ao_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImageCreateInfo snapshot_info = image_info;
  snapshot_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  snapshot_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  const VkResult snapshot_result = vmaCreateImage(
      m_allocator->getAllocator(), &snapshot_info, &alloc_info,
      &m_scene_snapshot_image, &m_scene_snapshot_allocation, nullptr);
  if (snapshot_result != VK_SUCCESS) {
    LOG_FATAL("[SsaOPass] vmaCreateImage scene snapshot failed: {}",
              static_cast<int>(snapshot_result));
  }

  view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_info.image = m_scene_snapshot_image;
  vkCreateImageView(device, &view_info, nullptr, &m_scene_snapshot_view);
  m_scene_snapshot_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void SsaOPass::destroyAoResources() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_ao_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, m_ao_framebuffer, nullptr);
    m_ao_framebuffer = VK_NULL_HANDLE;
  }
  if (m_ao_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_ao_image_view, nullptr);
    m_ao_image_view = VK_NULL_HANDLE;
  }
  if (m_ao_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), m_ao_image, m_ao_allocation);
    m_ao_image = VK_NULL_HANDLE;
    m_ao_allocation = VK_NULL_HANDLE;
  }
  m_ao_layout = VK_IMAGE_LAYOUT_UNDEFINED;

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

void SsaOPass::createDescriptorResources() {
  VkDevice device = m_context->getDevice();

  VkDescriptorSetLayoutBinding generate_bindings[5]{};
  generate_bindings[0].binding = 0;
  generate_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  generate_bindings[0].descriptorCount = 1;
  generate_bindings[0].stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  for (uint32_t pair = 0; pair < 2; ++pair) {
    generate_bindings[1 + pair * 2].binding = 1 + pair * 2;
    generate_bindings[1 + pair * 2].descriptorType =
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    generate_bindings[1 + pair * 2].descriptorCount = 1;
    generate_bindings[1 + pair * 2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    generate_bindings[2 + pair * 2].binding = 2 + pair * 2;
    generate_bindings[2 + pair * 2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    generate_bindings[2 + pair * 2].descriptorCount = 1;
    generate_bindings[2 + pair * 2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  VkDescriptorSetLayoutCreateInfo generate_layout_info{};
  generate_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  generate_layout_info.bindingCount = 5;
  generate_layout_info.pBindings = generate_bindings;
  vkCreateDescriptorSetLayout(device, &generate_layout_info, nullptr,
                              &m_generate_descriptor_layout);

  VkDescriptorSetLayoutBinding apply_bindings[5]{};
  apply_bindings[0] = generate_bindings[0];
  apply_bindings[1].binding = 1;
  apply_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  apply_bindings[1].descriptorCount = 1;
  apply_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  apply_bindings[2].binding = 2;
  apply_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  apply_bindings[2].descriptorCount = 1;
  apply_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  apply_bindings[3].binding = 3;
  apply_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  apply_bindings[3].descriptorCount = 1;
  apply_bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  apply_bindings[4].binding = 4;
  apply_bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  apply_bindings[4].descriptorCount = 1;
  apply_bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo apply_layout_info = generate_layout_info;
  apply_layout_info.pBindings = apply_bindings;
  vkCreateDescriptorSetLayout(device, &apply_layout_info, nullptr,
                              &m_apply_descriptor_layout);

  VkPipelineLayoutCreateInfo generate_pl_info{};
  generate_pl_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  generate_pl_info.setLayoutCount = 1;
  generate_pl_info.pSetLayouts = &m_generate_descriptor_layout;
  vkCreatePipelineLayout(device, &generate_pl_info, nullptr,
                         &m_generate_pipeline_layout);

  VkPipelineLayoutCreateInfo apply_pl_info = generate_pl_info;
  apply_pl_info.pSetLayouts = &m_apply_descriptor_layout;
  vkCreatePipelineLayout(device, &apply_pl_info, nullptr,
                         &m_apply_pipeline_layout);

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 6},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 6},
  };
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = 2;
  pool_info.poolSizeCount = 3;
  pool_info.pPoolSizes = pool_sizes;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool);

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = m_descriptor_pool;
  alloc_info.descriptorSetCount = 1;

  alloc_info.pSetLayouts = &m_generate_descriptor_layout;
  vkAllocateDescriptorSets(device, &alloc_info, &m_generate_descriptor_set);

  alloc_info.pSetLayouts = &m_apply_descriptor_layout;
  vkAllocateDescriptorSets(device, &alloc_info, &m_apply_descriptor_set);

  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = m_uniform_buffer->getBuffer();
  ubo_info.range = sizeof(SsaOUniformData);

  VkWriteDescriptorSet ubo_writes[2]{};
  for (uint32_t i = 0; i < 2; ++i) {
    ubo_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_writes[i].dstBinding = 0;
    ubo_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_writes[i].descriptorCount = 1;
    ubo_writes[i].pBufferInfo = &ubo_info;
  }
  ubo_writes[0].dstSet = m_generate_descriptor_set;
  ubo_writes[1].dstSet = m_apply_descriptor_set;
  vkUpdateDescriptorSets(device, 2, ubo_writes, 0, nullptr);

  if (m_noise_texture) {
    VkDescriptorImageInfo noise_image_info{};
    noise_image_info.imageView = m_noise_texture->getImageView();
    noise_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo noise_sampler_info{};
    noise_sampler_info.sampler = m_noise_texture->getSampler();

    VkWriteDescriptorSet noise_writes[2]{};
    noise_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    noise_writes[0].dstSet = m_generate_descriptor_set;
    noise_writes[0].dstBinding = 3;
    noise_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    noise_writes[0].descriptorCount = 1;
    noise_writes[0].pImageInfo = &noise_image_info;
    noise_writes[1] = noise_writes[0];
    noise_writes[1].dstBinding = 4;
    noise_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    noise_writes[1].pImageInfo = &noise_sampler_info;
    vkUpdateDescriptorSets(device, 2, noise_writes, 0, nullptr);
  }
}

void SsaOPass::destroyDescriptorResources() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  m_generate_descriptor_set = VK_NULL_HANDLE;
  m_apply_descriptor_set = VK_NULL_HANDLE;
  if (m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
  }
  if (m_apply_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_apply_pipeline_layout, nullptr);
    m_apply_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_generate_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_generate_pipeline_layout, nullptr);
    m_generate_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_apply_descriptor_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_apply_descriptor_layout, nullptr);
    m_apply_descriptor_layout = VK_NULL_HANDLE;
  }
  if (m_generate_descriptor_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_generate_descriptor_layout, nullptr);
    m_generate_descriptor_layout = VK_NULL_HANDLE;
  }
}

void SsaOPass::createPipelines() {
  VkDevice device = m_context->getDevice();

  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back(
      {"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  {
    auto stages = VulkanShader::loadFromSlang(
        device, m_compiler, "engine/shaders/ssao_generate.slang", entries);
    m_generate_pipeline =
        createFullscreenPipeline(device, m_ao_render_pass,
                                 m_generate_pipeline_layout, stages);
    for (auto& stage : stages) {
      VulkanShader::destroyShaderModule(device, &stage.module);
    }
  }

  {
    auto stages = VulkanShader::loadFromSlang(device, m_compiler,
                                              "engine/shaders/ssao_apply.slang",
                                              entries);
    m_apply_pipeline = createFullscreenPipeline(
        device, m_composite_render_pass, m_apply_pipeline_layout, stages);
    for (auto& stage : stages) {
      VulkanShader::destroyShaderModule(device, &stage.module);
    }
  }
}

void SsaOPass::destroyPipelines() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_apply_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_apply_pipeline, nullptr);
    m_apply_pipeline = VK_NULL_HANDLE;
  }
  if (m_generate_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_generate_pipeline, nullptr);
    m_generate_pipeline = VK_NULL_HANDLE;
  }
}

void SsaOPass::writeGenerateDescriptors(OffscreenRenderTarget* offscreen) {
  if (!offscreen) {
    return;
  }
  VkDevice device = m_context->getDevice();

  VkDescriptorImageInfo depth_image_info{};
  depth_image_info.imageView = offscreen->getDepthImageView();
  depth_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo depth_sampler_info{};
  depth_sampler_info.sampler = m_depth_sampler;

  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_generate_descriptor_set;
  writes[0].dstBinding = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[0].descriptorCount = 1;
  writes[0].pImageInfo = &depth_image_info;
  writes[1] = writes[0];
  writes[1].dstBinding = 2;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[1].pImageInfo = &depth_sampler_info;
  vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
}

void SsaOPass::copySceneSnapshot(VkCommandBuffer cmd,
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

void SsaOPass::writeApplyDescriptors() {
  if (m_ao_image_view == VK_NULL_HANDLE ||
      m_scene_snapshot_view == VK_NULL_HANDLE) {
    return;
  }
  VkDevice device = m_context->getDevice();

  VkDescriptorImageInfo scene_image_info{};
  scene_image_info.imageView = m_scene_snapshot_view;
  scene_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo scene_sampler_info{};
  scene_sampler_info.sampler = m_linear_sampler;

  VkDescriptorImageInfo ao_image_info{};
  ao_image_info.imageView = m_ao_image_view;
  ao_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo ao_sampler_info{};
  ao_sampler_info.sampler = m_linear_sampler;

  VkWriteDescriptorSet writes[4]{};
  for (uint32_t i = 0; i < 4; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = m_apply_descriptor_set;
    writes[i].descriptorCount = 1;
  }
  writes[0].dstBinding = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[0].pImageInfo = &scene_image_info;
  writes[1].dstBinding = 2;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[1].pImageInfo = &scene_sampler_info;
  writes[2].dstBinding = 3;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[2].pImageInfo = &ao_image_info;
  writes[3].dstBinding = 4;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[3].pImageInfo = &ao_sampler_info;
  vkUpdateDescriptorSets(device, 4, writes, 0, nullptr);
}

void SsaOPass::uploadUniforms(const BlinnPhongEditorSettings& settings,
                              const glm::mat4& projection, float near_clip,
                              float far_clip, uint32_t width, uint32_t height) {
  SsaOUniformData ubo{};
  ubo.projection = projection;
  ubo.inv_projection = glm::inverse(projection);
  ubo.clip_params =
      glm::vec4(near_clip, far_clip, settings.ssao_radius, settings.ssao_bias);
  ubo.screen_params = glm::vec4(1.0f / static_cast<float>(width),
                                1.0f / static_cast<float>(height),
                                static_cast<float>(width),
                                static_cast<float>(height));
  ubo.effect_params =
      glm::vec4(settings.ssao_enabled ? 1.0f : 0.0f, settings.ssao_strength,
                2.0f, 0.0f);
  m_uniform_buffer->upload(&ubo, sizeof(ubo));
}

void SsaOPass::apply(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen,
                     const BlinnPhongEditorSettings& settings,
                     const glm::mat4& projection, float near_clip,
                     float far_clip) {
  if (!offscreen || !m_generate_pipeline || m_width == 0 || m_height == 0) {
    return;
  }

  const VkExtent2D extent = offscreen->getExtent();
  if (extent.width != m_width || extent.height != m_height) {
    resize(extent.width, extent.height);
  }
  if (m_ao_framebuffer == VK_NULL_HANDLE) {
    return;
  }

  if (m_composite_framebuffer == VK_NULL_HANDLE) {
    VkImageView color_attachment = offscreen->getImageView();
    VkFramebufferCreateInfo fb_info{};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = m_composite_render_pass;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = &color_attachment;
    fb_info.width = m_width;
    fb_info.height = m_height;
    fb_info.layers = 1;
    vkCreateFramebuffer(m_context->getDevice(), &fb_info, nullptr,
                        &m_composite_framebuffer);
  }

  uploadUniforms(settings, projection, near_clip, far_clip, m_width, m_height);
  writeGenerateDescriptors(offscreen);
  writeApplyDescriptors();

  offscreen->cmdBarrierDepthToShaderRead(cmd);

  if (!settings.ssao_enabled) {
    return;
  }

  VkViewport viewport{};
  viewport.width = static_cast<float>(m_width);
  viewport.height = static_cast<float>(m_height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, {m_width, m_height}};

  if (m_ao_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
      m_ao_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
    VkImageMemoryBarrier ao_barrier{};
    ao_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ao_barrier.oldLayout = m_ao_layout;
    ao_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ao_barrier.image = m_ao_image;
    ao_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ao_barrier.subresourceRange.levelCount = 1;
    ao_barrier.subresourceRange.layerCount = 1;
    ao_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ao_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &ao_barrier);
  }

  VkClearValue ao_clear{};
  ao_clear.color = {{1.0f, 1.0f, 1.0f, 1.0f}};
  VkRenderPassBeginInfo ao_begin{};
  ao_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  ao_begin.renderPass = m_ao_render_pass;
  ao_begin.framebuffer = m_ao_framebuffer;
  ao_begin.renderArea.extent = {m_width, m_height};
  ao_begin.clearValueCount = 1;
  ao_begin.pClearValues = &ao_clear;
  vkCmdBeginRenderPass(cmd, &ao_begin, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_generate_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_generate_pipeline_layout, 0, 1,
                          &m_generate_descriptor_set, 0, nullptr);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);
  m_ao_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  copySceneSnapshot(cmd, offscreen);
  static bool s_logged_snapshot_composite = false;
  if (!s_logged_snapshot_composite) {
    LOG_INFO(
        "[SsaOPass] SSAO composite uses scene snapshot (no offscreen feedback)");
    s_logged_snapshot_composite = true;
  }

  VkClearValue composite_clear{};
  composite_clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  VkRenderPassBeginInfo composite_begin{};
  composite_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  composite_begin.renderPass = m_composite_render_pass;
  composite_begin.framebuffer = m_composite_framebuffer;
  composite_begin.renderArea.extent = {m_width, m_height};
  composite_begin.clearValueCount = 1;
  composite_begin.pClearValues = &composite_clear;
  vkCmdBeginRenderPass(cmd, &composite_begin, VK_SUBPASS_CONTENTS_INLINE);
  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_apply_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_apply_pipeline_layout, 0, 1,
                          &m_apply_descriptor_set, 0, nullptr);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);

  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

}  // namespace Blunder
