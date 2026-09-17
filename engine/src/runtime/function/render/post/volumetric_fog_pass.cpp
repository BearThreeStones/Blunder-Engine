#include "runtime/function/render/post/volumetric_fog_pass.h"

#include <algorithm>
#include <cmath>

#include <slang.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <vk_mem_alloc.h>

#include "EASTL/unique_ptr.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/secondary_command_buffer_pool.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/viewport_style.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

namespace {

struct VolumetricFogUniformData {
  glm::mat4 inv_view_proj{1.0f};
  glm::mat4 view_proj{1.0f};
  glm::mat4 prev_view_proj{1.0f};
  glm::vec4 camera_pos_far{0.0f};
  glm::vec4 grid_z_params{0.0f};
  glm::vec4 density_params{0.0f};
  glm::vec4 fog_albedo_g{1.0f, 1.0f, 1.0f, 0.2f};
  glm::vec4 light_color{0.0f};
  glm::vec4 light_dir{0.0f, 0.0f, -1.0f, 0.0f};
  glm::vec4 grid_size{1.0f};
  glm::vec4 jitter_temporal{0.0f, 0.0f, k_volumetric_fog_temporal_current, 0.0f};
  glm::vec4 local_light_count{0.0f};
  GpuSceneLight local_lights[k_max_volumetric_fog_local_lights];
};

static_assert(sizeof(GpuSceneLight) == 80,
              "FogLocalLight / GpuSceneLight must stay 5 float4s");
static_assert(sizeof(VolumetricFogUniformData) == 976,
              "VolumetricFogUniforms CPU/GPU size must match slang std140");
static_assert(OffscreenRenderTarget::k_buffer_count == 2 &&
                  VulkanSync::k_max_frames_in_flight == 2,
              "Fog FIF slots must match the offscreen color ping-pong");

float halton(uint32_t index, uint32_t base) {
  float f = 1.0f;
  float r = 0.0f;
  uint32_t i = index;
  while (i > 0) {
    f /= static_cast<float>(base);
    r += f * static_cast<float>(i % base);
    i /= base;
  }
  return r;
}

VkPipeline createComputePipeline(VkDevice device, VkPipelineLayout layout,
                                 VkShaderModule module) {
  VkPipelineShaderStageCreateInfo stage{};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = module;
  stage.pName = "main";

  VkComputePipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  info.stage = stage;
  info.layout = layout;

  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result =
      vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VolumetricFogPass] vkCreateComputePipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

VkPipeline createFullscreenPipeline(
    VulkanContext* context, VkRenderPass render_pass, VkPipelineLayout layout,
    const eastl::vector<VulkanShader::ShaderStage>& stages) {
  eastl::vector<VkPipelineShaderStageCreateInfo> stage_infos;
  stage_infos.reserve(stages.size());
  for (const auto& stage : stages) {
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
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                    VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT |
                                    VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
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
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result =
      context->createGraphicsPipelines(1, &pipeline_info, &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VolumetricFogPass] vkCreateGraphicsPipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

}  // namespace

VolumetricFogPass::~VolumetricFogPass() { shutdown(); }

void VolumetricFogPass::initialize(VulkanContext* context,
                                   VulkanAllocator* allocator,
                                   SlangCompiler* compiler) {
  ASSERT(context);
  ASSERT(allocator);
  ASSERT(compiler);
  m_context = context;
  m_allocator = allocator;
  m_compiler = compiler;

  VkDevice device = m_context->getDevice();
  VkSamplerCreateInfo sampler{};
  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(device, &sampler, nullptr, &m_linear_sampler);

  sampler.magFilter = VK_FILTER_NEAREST;
  sampler.minFilter = VK_FILTER_NEAREST;
  vkCreateSampler(device, &sampler, nullptr, &m_depth_sampler);

  for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
    m_uniform_buffer[slot] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffer[slot]->create(m_allocator, sizeof(VolumetricFogUniformData),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  createCompositePass();
  createDescriptorResources();
  createPipelines();
}

void VolumetricFogPass::shutdown() {
  if (m_context == nullptr) {
    return;
  }
  destroyPipelines();
  destroyDescriptorResources();
  destroyCompositePass();
  destroyVolumes();

  VkDevice device = m_context->getDevice();
  if (m_linear_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_linear_sampler, nullptr);
    m_linear_sampler = VK_NULL_HANDLE;
  }
  if (m_depth_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_depth_sampler, nullptr);
    m_depth_sampler = VK_NULL_HANDLE;
  }
  for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
    if (m_uniform_buffer[slot]) {
      m_uniform_buffer[slot]->destroy();
      m_uniform_buffer[slot].reset();
    }
  }
  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
  m_history_valid = false;
}

void VolumetricFogPass::invalidateHistory() { m_history_valid = false; }

void VolumetricFogPass::createCompositePass() {
  VkAttachmentDescription color{};
  color.format = VK_FORMAT_R8G8B8A8_UNORM;
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 1;
  info.pAttachments = &color;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;
  vkCreateRenderPass(m_context->getDevice(), &info, nullptr, &m_composite_render_pass);
}

void VolumetricFogPass::invalidateSlotCaches() {
  for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
    m_density_desc_valid[slot] = false;
    m_composite_desc_valid[slot] = false;
    m_scatter_desc_history[slot] = ~0u;
    m_scatter_desc_current[slot] = ~0u;
    m_integrate_desc_scatter[slot] = ~0u;
    m_composite_depth_view[slot] = VK_NULL_HANDLE;
  }
}

void VolumetricFogPass::destroyCompositePass() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
    if (m_composite_framebuffer[slot] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, m_composite_framebuffer[slot], nullptr);
      m_composite_framebuffer[slot] = VK_NULL_HANDLE;
    }
    m_composite_color_view[slot] = VK_NULL_HANDLE;
  }
  if (m_composite_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_composite_render_pass, nullptr);
    m_composite_render_pass = VK_NULL_HANDLE;
  }
}

void VolumetricFogPass::createDescriptorResources() {
  VkDevice device = m_context->getDevice();

  auto make_layout = [&](const VkDescriptorSetLayoutBinding* bindings,
                         uint32_t count, VkDescriptorSetLayout* out) {
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = count;
    info.pBindings = bindings;
    vkCreateDescriptorSetLayout(device, &info, nullptr, out);
  };

  VkDescriptorSetLayoutBinding density_b[2]{};
  density_b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                  nullptr};
  density_b[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                  nullptr};
  make_layout(density_b, 2, &m_density_layout);

  VkDescriptorSetLayoutBinding scatter_b[6]{};
  scatter_b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                  nullptr};
  scatter_b[1] = {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                  nullptr};
  scatter_b[2] = {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  scatter_b[3] = {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                  nullptr};
  scatter_b[4] = {4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  scatter_b[5] = {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                  nullptr};
  make_layout(scatter_b, 6, &m_scatter_layout);

  VkDescriptorSetLayoutBinding integrate_b[4]{};
  integrate_b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                    nullptr};
  integrate_b[1] = {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                    nullptr};
  integrate_b[2] = {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  integrate_b[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                    nullptr};
  make_layout(integrate_b, 4, &m_integrate_layout);

  VkDescriptorSetLayoutBinding composite_b[7]{};
  composite_b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                    VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr};
  composite_b[1] = {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr};
  composite_b[2] = {2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  composite_b[3] = {3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr};
  composite_b[4] = {4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  composite_b[5] = {5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                    nullptr};
  composite_b[6] = {6, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
  make_layout(composite_b, 7, &m_composite_layout);

  auto make_pipeline_layout = [&](VkDescriptorSetLayout set_layout,
                                  VkPipelineLayout* out) {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 1;
    info.pSetLayouts = &set_layout;
    vkCreatePipelineLayout(device, &info, nullptr, out);
  };
  make_pipeline_layout(m_density_layout, &m_density_pipeline_layout);
  make_pipeline_layout(m_scatter_layout, &m_scatter_pipeline_layout);
  make_pipeline_layout(m_integrate_layout, &m_integrate_pipeline_layout);
  make_pipeline_layout(m_composite_layout, &m_composite_pipeline_layout);

  VkDescriptorPoolSize pool_sizes[4]{};
  pool_sizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4u * k_fog_frames};
  pool_sizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4u * k_fog_frames};
  pool_sizes[2] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8u * k_fog_frames};
  pool_sizes[3] = {VK_DESCRIPTOR_TYPE_SAMPLER, 8u * k_fog_frames};
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 4;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 4u * k_fog_frames;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool);

  VkDescriptorSetLayout layouts[4u * k_fog_frames]{};
  for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
    layouts[slot * 4u + 0u] = m_density_layout;
    layouts[slot * 4u + 1u] = m_scatter_layout;
    layouts[slot * 4u + 2u] = m_integrate_layout;
    layouts[slot * 4u + 3u] = m_composite_layout;
  }
  VkDescriptorSetAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc.descriptorPool = m_descriptor_pool;
  alloc.descriptorSetCount = 4u * k_fog_frames;
  alloc.pSetLayouts = layouts;
  VkDescriptorSet sets[4u * k_fog_frames]{};
  vkAllocateDescriptorSets(device, &alloc, sets);
  for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
    m_density_set[slot] = sets[slot * 4u + 0u];
    m_scatter_set[slot] = sets[slot * 4u + 1u];
    m_integrate_set[slot] = sets[slot * 4u + 2u];
    m_composite_set[slot] = sets[slot * 4u + 3u];
  }
  invalidateSlotCaches();
}

void VolumetricFogPass::destroyDescriptorResources() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
    for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
      m_density_set[slot] = VK_NULL_HANDLE;
      m_scatter_set[slot] = VK_NULL_HANDLE;
      m_integrate_set[slot] = VK_NULL_HANDLE;
      m_composite_set[slot] = VK_NULL_HANDLE;
    }
    invalidateSlotCaches();
  }
  auto destroy_layout = [&](VkPipelineLayout& layout) {
    if (layout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device, layout, nullptr);
      layout = VK_NULL_HANDLE;
    }
  };
  destroy_layout(m_density_pipeline_layout);
  destroy_layout(m_scatter_pipeline_layout);
  destroy_layout(m_integrate_pipeline_layout);
  destroy_layout(m_composite_pipeline_layout);
  auto destroy_set_layout = [&](VkDescriptorSetLayout& layout) {
    if (layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device, layout, nullptr);
      layout = VK_NULL_HANDLE;
    }
  };
  destroy_set_layout(m_density_layout);
  destroy_set_layout(m_scatter_layout);
  destroy_set_layout(m_integrate_layout);
  destroy_set_layout(m_composite_layout);
}

void VolumetricFogPass::createPipelines() {
  VkDevice device = m_context->getDevice();
  eastl::vector<VulkanShader::EntryPointSpec> cs;
  cs.push_back({"main", VK_SHADER_STAGE_COMPUTE_BIT, SLANG_STAGE_COMPUTE});

  auto load_cs = [&](const char* path, VkPipelineLayout layout) {
    auto stages = VulkanShader::loadFromSlang(device, m_compiler, path, cs);
    VkPipeline pipeline =
        createComputePipeline(device, layout, stages.front().module);
    for (auto& stage : stages) {
      VulkanShader::destroyShaderModule(device, &stage.module);
    }
    return pipeline;
  };

  m_density_pipeline =
      load_cs("engine/shaders/volumetric_fog_density.slang", m_density_pipeline_layout);
  m_scatter_pipeline =
      load_cs("engine/shaders/volumetric_fog_scatter.slang", m_scatter_pipeline_layout);
  m_integrate_pipeline = load_cs("engine/shaders/volumetric_fog_integrate.slang",
                                 m_integrate_pipeline_layout);

  eastl::vector<VulkanShader::EntryPointSpec> gfx;
  gfx.push_back({"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  gfx.push_back({"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});
  auto stages = VulkanShader::loadFromSlang(
      device, m_compiler, "engine/shaders/volumetric_fog_composite.slang", gfx);
  m_composite_pipeline = createFullscreenPipeline(
      m_context, m_composite_render_pass, m_composite_pipeline_layout, stages);
  for (auto& stage : stages) {
    VulkanShader::destroyShaderModule(device, &stage.module);
  }
}

void VolumetricFogPass::destroyPipelines() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  auto destroy = [&](VkPipeline& pipeline) {
    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(device, pipeline, nullptr);
      pipeline = VK_NULL_HANDLE;
    }
  };
  destroy(m_density_pipeline);
  destroy(m_scatter_pipeline);
  destroy(m_integrate_pipeline);
  destroy(m_composite_pipeline);
}

void VolumetricFogPass::destroyVolumes() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  auto destroy_volume = [&](Volume3D& volume) {
    if (volume.view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, volume.view, nullptr);
      volume.view = VK_NULL_HANDLE;
    }
    if (volume.image != VK_NULL_HANDLE) {
      vmaDestroyImage(m_allocator->getAllocator(), volume.image, volume.allocation);
      volume.image = VK_NULL_HANDLE;
      volume.allocation = VK_NULL_HANDLE;
    }
    volume.layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };
  destroy_volume(m_density[0]);
  destroy_volume(m_density[1]);
  destroy_volume(m_scatter[0]);
  destroy_volume(m_scatter[1]);
  destroy_volume(m_integrated[0]);
  destroy_volume(m_integrated[1]);

  if (m_scene_snapshot_view[0] != VK_NULL_HANDLE ||
      m_scene_snapshot_image[0] != VK_NULL_HANDLE) {
    for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
      if (m_scene_snapshot_view[slot] != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_scene_snapshot_view[slot], nullptr);
        m_scene_snapshot_view[slot] = VK_NULL_HANDLE;
      }
      if (m_scene_snapshot_image[slot] != VK_NULL_HANDLE) {
        vmaDestroyImage(m_allocator->getAllocator(), m_scene_snapshot_image[slot],
                        m_scene_snapshot_allocation[slot]);
        m_scene_snapshot_image[slot] = VK_NULL_HANDLE;
        m_scene_snapshot_allocation[slot] = VK_NULL_HANDLE;
      }
      m_scene_snapshot_layout[slot] = VK_IMAGE_LAYOUT_UNDEFINED;
    }
  }
  if (m_context != nullptr) {
    VkDevice device = m_context->getDevice();
    for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
      if (m_composite_framebuffer[slot] != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_composite_framebuffer[slot], nullptr);
        m_composite_framebuffer[slot] = VK_NULL_HANDLE;
      }
      m_composite_color_view[slot] = VK_NULL_HANDLE;
    }
  }
  invalidateSlotCaches();
  m_view_width = 0;
  m_view_height = 0;
  m_history_valid = false;
}

void VolumetricFogPass::ensureVolumes(uint32_t width, uint32_t height) {
  const FroxelGridSize grid = froxelGridSize(width, height);
  if (width == m_view_width && height == m_view_height && m_density[0].image != VK_NULL_HANDLE) {
    m_grid = grid;
    return;
  }
  destroyVolumes();
  m_view_width = width;
  m_view_height = height;
  m_grid = grid;

  VkDevice device = m_context->getDevice();
  auto create_volume = [&](Volume3D& volume) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_3D;
    info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    info.extent = {grid.x, grid.y, grid.z};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo alloc{};
    alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    const VkResult result = vmaCreateImage(m_allocator->getAllocator(), &info, &alloc,
                                           &volume.image, &volume.allocation, nullptr);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[VolumetricFogPass] vmaCreateImage 3D failed: {}",
                static_cast<int>(result));
    }
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = volume.image;
    view.viewType = VK_IMAGE_VIEW_TYPE_3D;
    view.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &view, nullptr, &volume.view);
    volume.layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };
  create_volume(m_density[0]);
  create_volume(m_density[1]);
  create_volume(m_scatter[0]);
  create_volume(m_scatter[1]);
  create_volume(m_integrated[0]);
  create_volume(m_integrated[1]);

  VkImageCreateInfo snapshot{};
  snapshot.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  snapshot.imageType = VK_IMAGE_TYPE_2D;
  snapshot.format = VK_FORMAT_R8G8B8A8_UNORM;
  snapshot.extent = {width, height, 1};
  snapshot.mipLevels = 1;
  snapshot.arrayLayers = 1;
  snapshot.samples = VK_SAMPLE_COUNT_1_BIT;
  snapshot.tiling = VK_IMAGE_TILING_OPTIMAL;
  snapshot.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  snapshot.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  for (uint32_t slot = 0; slot < k_fog_frames; ++slot) {
    vmaCreateImage(m_allocator->getAllocator(), &snapshot, &alloc,
                   &m_scene_snapshot_image[slot], &m_scene_snapshot_allocation[slot],
                   nullptr);
    VkImageViewCreateInfo view{};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image = m_scene_snapshot_image[slot];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R8G8B8A8_UNORM;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &view, nullptr, &m_scene_snapshot_view[slot]);
    m_scene_snapshot_layout[slot] = VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

void VolumetricFogPass::barrierVolume(VkCommandBuffer cmd, Volume3D& volume,
                                      VkImageLayout new_layout,
                                      VkAccessFlags src_access,
                                      VkAccessFlags dst_access,
                                      VkPipelineStageFlags src_stage,
                                      VkPipelineStageFlags dst_stage) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = src_access;
  barrier.dstAccessMask = dst_access;
  barrier.oldLayout = volume.layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = volume.image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
  volume.layout = new_layout;
}

void VolumetricFogPass::writeDensityDescriptors(uint32_t slot) {
  VkDescriptorBufferInfo ubo{};
  ubo.buffer = m_uniform_buffer[slot]->getBuffer();
  ubo.range = sizeof(VolumetricFogUniformData);
  VkDescriptorImageInfo image{};
  image.imageView = m_density[slot].view;
  image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_density_set[slot];
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubo;
  writes[1] = writes[0];
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writes[1].pImageInfo = &image;
  vkUpdateDescriptorSets(m_context->getDevice(), 2, writes, 0, nullptr);
}

void VolumetricFogPass::writeScatterDescriptors(uint32_t slot, uint32_t history_index,
                                                uint32_t current_index) {
  VkDescriptorBufferInfo ubo{};
  ubo.buffer = m_uniform_buffer[slot]->getBuffer();
  ubo.range = sizeof(VolumetricFogUniformData);
  VkDescriptorImageInfo density{};
  density.imageView = m_density[slot].view;
  density.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo density_sampler{};
  density_sampler.sampler = m_linear_sampler;
  VkDescriptorImageInfo history{};
  history.imageView = m_scatter[history_index].view;
  history.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo history_sampler{};
  history_sampler.sampler = m_linear_sampler;
  VkDescriptorImageInfo current{};
  current.imageView = m_scatter[current_index].view;
  current.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkWriteDescriptorSet writes[6]{};
  for (uint32_t i = 0; i < 6; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = m_scatter_set[slot];
    writes[i].descriptorCount = 1;
  }
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &ubo;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &density;
  writes[2].dstBinding = 2;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[2].pImageInfo = &density_sampler;
  writes[3].dstBinding = 3;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[3].pImageInfo = &history;
  writes[4].dstBinding = 4;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[4].pImageInfo = &history_sampler;
  writes[5].dstBinding = 5;
  writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writes[5].pImageInfo = &current;
  vkUpdateDescriptorSets(m_context->getDevice(), 6, writes, 0, nullptr);
}

void VolumetricFogPass::writeIntegrateDescriptors(uint32_t slot,
                                                  uint32_t scatter_index) {
  VkDescriptorBufferInfo ubo{};
  ubo.buffer = m_uniform_buffer[slot]->getBuffer();
  ubo.range = sizeof(VolumetricFogUniformData);
  VkDescriptorImageInfo scatter{};
  scatter.imageView = m_scatter[scatter_index].view;
  scatter.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo sampler{};
  sampler.sampler = m_linear_sampler;
  VkDescriptorImageInfo integrated{};
  integrated.imageView = m_integrated[slot].view;
  integrated.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  VkWriteDescriptorSet writes[4]{};
  for (uint32_t i = 0; i < 4; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = m_integrate_set[slot];
    writes[i].descriptorCount = 1;
  }
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &ubo;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &scatter;
  writes[2].dstBinding = 2;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[2].pImageInfo = &sampler;
  writes[3].dstBinding = 3;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writes[3].pImageInfo = &integrated;
  vkUpdateDescriptorSets(m_context->getDevice(), 4, writes, 0, nullptr);
}

void VolumetricFogPass::writeCompositeDescriptors(uint32_t slot,
                                                  OffscreenRenderTarget* offscreen) {
  VkDescriptorBufferInfo ubo{};
  ubo.buffer = m_uniform_buffer[slot]->getBuffer();
  ubo.range = sizeof(VolumetricFogUniformData);
  VkDescriptorImageInfo scene{};
  scene.imageView = m_scene_snapshot_view[slot];
  scene.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo scene_sampler{};
  scene_sampler.sampler = m_linear_sampler;
  VkDescriptorImageInfo depth{};
  depth.imageView = offscreen->getDepthImageView();
  depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo depth_sampler{};
  depth_sampler.sampler = m_depth_sampler;
  VkDescriptorImageInfo integrated{};
  integrated.imageView = m_integrated[slot].view;
  integrated.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo integrated_sampler{};
  integrated_sampler.sampler = m_linear_sampler;

  VkWriteDescriptorSet writes[7]{};
  for (uint32_t i = 0; i < 7; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = m_composite_set[slot];
    writes[i].descriptorCount = 1;
  }
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &ubo;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &scene;
  writes[2].dstBinding = 2;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[2].pImageInfo = &scene_sampler;
  writes[3].dstBinding = 3;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[3].pImageInfo = &depth;
  writes[4].dstBinding = 4;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[4].pImageInfo = &depth_sampler;
  writes[5].dstBinding = 5;
  writes[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[5].pImageInfo = &integrated;
  writes[6].dstBinding = 6;
  writes[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[6].pImageInfo = &integrated_sampler;
  vkUpdateDescriptorSets(m_context->getDevice(), 7, writes, 0, nullptr);
}

void VolumetricFogPass::copySceneSnapshot(VkCommandBuffer cmd,
                                          OffscreenRenderTarget* offscreen,
                                          uint32_t slot) {
  VkImage src = offscreen->getImage();
  VkImage dst = m_scene_snapshot_image[slot];
  const VkImageLayout src_layout = offscreen->getCurrentLayout();

  // Always wait on the deferred lighting / LOAD overlay color write. Tracking
  // SHADER_READ_ONLY on the CPU after vkCmdEndRenderPass is not enough — a
  // FRAGMENT-only barrier can copy the previous scene and the composite
  // DONT_CARE pass then overwrites Sponza lighting with those pixels.
  VkImageMemoryBarrier barriers[2]{};
  barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].image = src;
  barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barriers[0].subresourceRange.levelCount = 1;
  barriers[0].subresourceRange.layerCount = 1;
  barriers[0].oldLayout = src_layout == VK_IMAGE_LAYOUT_UNDEFINED
                              ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                              : src_layout;
  barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barriers[0].srcAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
  barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barriers[1] = barriers[0];
  barriers[1].image = dst;
  barriers[1].oldLayout = m_scene_snapshot_layout[slot];
  barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barriers[1].srcAccessMask = 0;
  barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2,
                       barriers);

  VkImageCopy copy{};
  copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.srcSubresource.layerCount = 1;
  copy.dstSubresource = copy.srcSubresource;
  copy.extent = {m_view_width, m_view_height, 1};
  vkCmdCopyImage(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  VkImageMemoryBarrier post[2]{};
  post[0] = barriers[0];
  post[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  post[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  post[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  post[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  post[1] = barriers[1];
  post[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  post[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  post[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  post[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 2, post);
  m_scene_snapshot_layout[slot] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VolumetricFogPass::dispatchCompute(VkCommandBuffer cmd, VkPipeline pipeline,
                                        VkPipelineLayout layout, VkDescriptorSet set,
                                        uint32_t groups_x, uint32_t groups_y,
                                        uint32_t groups_z) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0,
                          nullptr);
  vkCmdDispatch(cmd, std::max(1u, groups_x), std::max(1u, groups_y),
                std::max(1u, groups_z));
}

void VolumetricFogPass::apply(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen,
                              const ForwardFrameState& frame_state, const ActiveFog& fog,
                              uint32_t frame_index) {
  if (offscreen == nullptr || m_density_pipeline == VK_NULL_HANDLE ||
      !isValid(fog.entity_id)) {
    return;
  }
  const VkExtent2D extent = offscreen->getExtent();
  ensureVolumes(extent.width, extent.height);
  if (m_density[0].image == VK_NULL_HANDLE) {
    return;
  }

  const glm::mat4 view_proj = frame_state.projection * frame_state.view;
  const glm::vec3 view_forward = volumetricFogViewForward(frame_state.view);
  const bool reuse_history =
      m_history_valid && volumetricFogHistoryUseful(m_prev_view_forward, view_forward);
  const bool centimetre_world =
      frame_state.lighting_scene != nullptr &&
      sceneIsCentimetreWorld(*frame_state.lighting_scene);
  const float near_z = volumetricFogNear(frame_state.near_clip, centimetre_world);
  // Fog Unique viewDistance, not camera far. Viewport far 100000 would spread
  // 64 slices across 100 km and the courtyard density would vanish.
  const float far_z = volumetricFogVolumeFar(fog.fog.view_distance, centimetre_world);
  const FroxelGridZParams z_params = makeFroxelGridZParams(near_z, far_z, m_grid.z);
  const uint32_t slot = frame_index % k_fog_frames;

  VolumetricFogUniformData ubo{};
  ubo.view_proj = view_proj;
  ubo.inv_view_proj = glm::inverse(view_proj);
  ubo.prev_view_proj = reuse_history ? m_prev_view_proj : view_proj;
  ubo.camera_pos_far = glm::vec4(frame_state.camera_position, far_z);
  ubo.grid_z_params = glm::vec4(z_params.x, z_params.y, z_params.z, near_z);
  ubo.density_params =
      glm::vec4(volumetricFogDensity(fog.fog.density, centimetre_world),
                volumetricFogHeightFalloff(fog.fog.height_falloff, centimetre_world),
                fog.world_height_z, volumetricFogLocalSourceRadius(centimetre_world));
  ubo.fog_albedo_g = glm::vec4(fog.fog.albedo, fog.fog.scattering_g);
  ubo.grid_size = glm::vec4(static_cast<float>(m_grid.x), static_cast<float>(m_grid.y),
                            static_cast<float>(m_grid.z), 0.0f);
  const float jitter_x = reuse_history ? (halton(frame_index + 1, 2) - 0.5f) : 0.0f;
  const float jitter_y = reuse_history ? (halton(frame_index + 1, 3) - 0.5f) : 0.0f;
  ubo.jitter_temporal =
      glm::vec4(jitter_x, jitter_y, k_volumetric_fog_temporal_current,
                reuse_history ? 1.0f : 0.0f);

  if (frame_state.lighting_scene != nullptr) {
    const EntityId dir_id = pickIlluminatingDirectional(*frame_state.lighting_scene);
    if (isValid(dir_id)) {
      if (const LightComponent* light = frame_state.lighting_scene->getLight(dir_id)) {
        EvaluatedLight evaluated{};
        fillEvaluatedLight(dir_id, *light,
                           frame_state.lighting_scene->getWorldMatrix(dir_id), evaluated);
        ubo.light_color = glm::vec4(evaluated.color_times_intensity, 1.0f);
        ubo.light_dir = glm::vec4(evaluated.world_emit, 0.0f);
      }
    }

    EvaluatedLight locals[k_max_volumetric_fog_local_lights];
    const size_t local_count = gatherFogLocalLights(
        *frame_state.lighting_scene, locals, k_max_volumetric_fog_local_lights);
    ubo.local_light_count = glm::vec4(static_cast<float>(local_count), 0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < local_count; ++i) {
      EvaluatedLight fog_light = locals[i];
      fog_light.color_times_intensity *= k_volumetric_fog_local_scatter_scale;
      packEvaluatedLight(ubo.local_lights[i], fog_light, k_invalid_entity_id, false,
                         nullptr);
    }
  }

  m_uniform_buffer[slot]->upload(&ubo, sizeof(ubo));

  const uint32_t current = m_history_index ^ 1u;
  const uint32_t history = m_history_index;

  barrierVolume(cmd, m_density[slot], VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  if (!m_density_desc_valid[slot]) {
    writeDensityDescriptors(slot);
    m_density_desc_valid[slot] = true;
  }
  dispatchCompute(cmd, m_density_pipeline, m_density_pipeline_layout, m_density_set[slot],
                  (m_grid.x + 3u) / 4u, (m_grid.y + 3u) / 4u, (m_grid.z + 3u) / 4u);
  barrierVolume(cmd, m_density[slot], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  if (m_scatter[history].layout == VK_IMAGE_LAYOUT_UNDEFINED) {
    barrierVolume(cmd, m_scatter[history], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0,
                  VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }
  barrierVolume(cmd, m_scatter[current], VK_IMAGE_LAYOUT_GENERAL, 0,
                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  if (m_scatter_desc_history[slot] != history || m_scatter_desc_current[slot] != current) {
    writeScatterDescriptors(slot, history, current);
    m_scatter_desc_history[slot] = history;
    m_scatter_desc_current[slot] = current;
  }
  dispatchCompute(cmd, m_scatter_pipeline, m_scatter_pipeline_layout, m_scatter_set[slot],
                  (m_grid.x + 3u) / 4u, (m_grid.y + 3u) / 4u, (m_grid.z + 3u) / 4u);
  barrierVolume(cmd, m_scatter[current], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  barrierVolume(cmd, m_integrated[slot], VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  if (m_integrate_desc_scatter[slot] != current) {
    writeIntegrateDescriptors(slot, current);
    m_integrate_desc_scatter[slot] = current;
  }
  dispatchCompute(cmd, m_integrate_pipeline, m_integrate_pipeline_layout,
                  m_integrate_set[slot], (m_grid.x + 7u) / 8u, (m_grid.y + 7u) / 8u, 1u);
  barrierVolume(cmd, m_integrated[slot], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  offscreen->cmdBarrierToShaderRead(cmd);
  offscreen->cmdBarrierDepthToShaderRead(cmd);
  copySceneSnapshot(cmd, offscreen, slot);
  const VkImageView depth_view = offscreen->getDepthImageView();
  if (!m_composite_desc_valid[slot] || m_composite_depth_view[slot] != depth_view) {
    writeCompositeDescriptors(slot, offscreen);
    m_composite_desc_valid[slot] = true;
    m_composite_depth_view[slot] = depth_view;
  }

  VkImageView color_view = offscreen->getImageView();
  if (m_composite_framebuffer[slot] == VK_NULL_HANDLE ||
      m_composite_color_view[slot] != color_view) {
    if (m_composite_framebuffer[slot] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(m_context->getDevice(), m_composite_framebuffer[slot],
                           nullptr);
      m_composite_framebuffer[slot] = VK_NULL_HANDLE;
    }
    VkFramebufferCreateInfo fb{};
    fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb.renderPass = m_composite_render_pass;
    fb.attachmentCount = 1;
    fb.pAttachments = &color_view;
    fb.width = m_view_width;
    fb.height = m_view_height;
    fb.layers = 1;
    vkCreateFramebuffer(m_context->getDevice(), &fb, nullptr,
                        &m_composite_framebuffer[slot]);
    m_composite_color_view[slot] = color_view;
  }

  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_composite_render_pass;
  begin.framebuffer = m_composite_framebuffer[slot];
  begin.renderArea.extent = {m_view_width, m_view_height};
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_composite_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_composite_pipeline_layout,
                          0, 1, &m_composite_set[slot], 0, nullptr);
  VkViewport viewport{};
  viewport.width = static_cast<float>(m_view_width);
  viewport.height = static_cast<float>(m_view_height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, {m_view_width, m_view_height}};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);
  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_prev_view_proj = view_proj;
  m_prev_view_forward = view_forward;
  m_history_index = current;
  m_history_valid = true;
}

}  // namespace Blunder
