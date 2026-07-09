#include "runtime/function/render/overlay/outline_overlay.h"

#include <slang.h>

#include <glm/vec4.hpp>

#include "EASTL/unique_ptr.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/overlay/outline_id_pack.h"
#include "runtime/function/render/overlay/outline_targets.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

namespace Blunder {

namespace {

struct OutlinePrepassUniformData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
  uint32_t packed_object_id{0};
  uint32_t _pad[3]{};
};

struct OutlineResolveUniformData {
  glm::vec4 screen_params;
};

bool isEntityOrDescendant(const SceneInstance& instance, EntityId root,
                          EntityId candidate) {
  EntityId current = candidate;
  while (isValid(current)) {
    if (current == root) {
      return true;
    }
    const Entity* entity = instance.getEntity(current);
    if (entity == nullptr) {
      break;
    }
    current = entity->getParentId();
  }
  return false;
}

VkPipeline createGraphicsPipeline(
    VkDevice device, VkRenderPass render_pass, VkPipelineLayout pipeline_layout,
    const eastl::vector<VulkanShader::ShaderStage>& stages,
    bool enable_blend, bool depth_test, bool depth_write,
    VkCullModeFlags cull_mode) {
  eastl::vector<VkPipelineShaderStageCreateInfo> stage_infos;
  for (const VulkanShader::ShaderStage& stage : stages) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage.stage_flags;
    info.module = stage.module;
    info.pName = stage.entry_point.c_str();
    stage_infos.push_back(info);
  }

  const VkVertexInputBindingDescription binding = Vertex::getBindingDescription();
  const auto attrs = Vertex::getAttributeDescriptions();
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = 1;
  vertex_input.pVertexBindingDescriptions = &binding;
  vertex_input.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attrs.size());
  vertex_input.pVertexAttributeDescriptions = attrs.data();

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
  rasterizer.cullMode = cull_mode;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = depth_test ? VK_TRUE : VK_FALSE;
  depth_stencil.depthWriteEnable = depth_write ? VK_TRUE : VK_FALSE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  if (enable_blend) {
    blend_attachment.blendEnable = VK_TRUE;
    blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  }

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
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OutlineOverlay] vkCreateGraphicsPipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

VkPipeline createFullscreenPipeline(
    VkDevice device, VkRenderPass render_pass, VkPipelineLayout pipeline_layout,
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

  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = VK_FALSE;
  depth_stencil.depthWriteEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.blendEnable = VK_TRUE;
  blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OutlineOverlay] resolve vkCreateGraphicsPipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

}  // namespace

OutlineOverlay::~OutlineOverlay() {
  shutdown();
}

void OutlineOverlay::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                                OffscreenRenderTarget* offscreen,
                                SlangCompiler* compiler,
                                OutlineTargets* targets) {
  ASSERT(ctx);
  ASSERT(alloc);
  ASSERT(offscreen);
  ASSERT(compiler);
  ASSERT(targets);
  m_context = ctx;
  m_allocator = alloc;
  m_offscreen = offscreen;
  m_compiler = compiler;
  m_targets = targets;

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(ctx->getDevice(), &sampler_info, nullptr, &m_depth_sampler);

  m_prepass_uniform_buffer = eastl::make_unique<VulkanBuffer>();
  m_prepass_uniform_buffer->create(alloc, sizeof(OutlinePrepassUniformData),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);

  m_resolve_uniform_buffer = eastl::make_unique<VulkanBuffer>();
  m_resolve_uniform_buffer->create(alloc, sizeof(OutlineResolveUniformData),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);

  createDescriptorResources();
  createResolveRenderPass();
  createPrepassPipeline();
  createResolvePipeline();

  const VkExtent2D extent = offscreen->getExtent();
  m_width = extent.width;
  m_height = extent.height;
}

void OutlineOverlay::shutdown() {
  destroyPrepassPipeline();
  destroyResolvePipeline();
  destroyDescriptorResources();
  destroyResolveRenderPass();

  if (m_context && m_depth_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_context->getDevice(), m_depth_sampler, nullptr);
    m_depth_sampler = VK_NULL_HANDLE;
  }
  if (m_prepass_uniform_buffer) {
    m_prepass_uniform_buffer->destroy();
    m_prepass_uniform_buffer.reset();
  }
  if (m_resolve_uniform_buffer) {
    m_resolve_uniform_buffer->destroy();
    m_resolve_uniform_buffer.reset();
  }

  m_targets = nullptr;
  m_offscreen = nullptr;
  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
  m_cached_draws = {};
  enabled_ = false;
}

void OutlineOverlay::resize(uint32_t width, uint32_t height) {
  m_width = width;
  m_height = height;
}

void OutlineOverlay::begin_sync(OverlayResources& /*res*/,
                                 const OverlayState& /*state*/) {
  m_cached_draws = {};

  if (!g_runtime_global_context.m_editor_selection ||
      !g_runtime_global_context.m_editor_selection->hasSelection() ||
      !g_runtime_global_context.m_scene_system ||
      !g_runtime_global_context.m_render_system) {
    enabled_ = false;
    return;
  }

  SceneInstance* scene =
      g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    enabled_ = false;
    return;
  }

  EditorSelectionSystem& selection = *g_runtime_global_context.m_editor_selection;
  RenderSystem* render_system = g_runtime_global_context.m_render_system.get();
  const uint32_t color_id = resolveOutlineColorId(
      render_system->isTranslateModalSessionActive(),
      render_system->isTransformGizmoDragging());

  const EntityId root_id = selection.getSelection();
  if (!isValid(root_id)) {
    enabled_ = false;
    return;
  }

  const uint16_t packed_id = OutlineIdPack::pack(color_id, 1u);

  auto try_add_draw = [&](EntityId entity_id,
                          const MeshRendererComponent& renderer) {
    if (!renderer.mesh) {
      return;
    }
    GpuMesh* gpu_mesh = render_system->getOrUploadGpuMesh(renderer.mesh.get());
    if (gpu_mesh == nullptr || gpu_mesh->getVertexBuffer() == nullptr ||
        gpu_mesh->getIndexBuffer() == nullptr ||
        gpu_mesh->getIndexCount() == 0) {
      return;
    }
    CachedDraw draw{};
    draw.gpu_mesh = gpu_mesh;
    draw.world_matrix = scene->getWorldMatrix(entity_id);
    draw.double_sided = renderer.double_sided;
    draw.packed_id = packed_id;
    m_cached_draws.push_back(draw);
  };

  if (const MeshRendererComponent* root_renderer = scene->getMeshRenderer(root_id)) {
    try_add_draw(root_id, *root_renderer);
  }

  scene->forEachMeshRenderer([&](EntityId entity_id,
                                 const MeshRendererComponent& renderer) {
    if (entity_id == root_id) {
      return;
    }
    if (!isEntityOrDescendant(*scene, root_id, entity_id)) {
      return;
    }
    try_add_draw(entity_id, renderer);
  });

  enabled_ = !m_cached_draws.empty();
}

void OutlineOverlay::createDescriptorResources() {
  VkDevice device = m_context->getDevice();

  VkDescriptorSetLayoutBinding prepass_binding{};
  prepass_binding.binding = 0;
  prepass_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  prepass_binding.descriptorCount = 1;
  prepass_binding.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo prepass_layout_info{};
  prepass_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  prepass_layout_info.bindingCount = 1;
  prepass_layout_info.pBindings = &prepass_binding;
  vkCreateDescriptorSetLayout(device, &prepass_layout_info, nullptr,
                              &m_prepass_descriptor_layout);

  VkPipelineLayoutCreateInfo prepass_pipeline_layout_info{};
  prepass_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  prepass_pipeline_layout_info.setLayoutCount = 1;
  prepass_pipeline_layout_info.pSetLayouts = &m_prepass_descriptor_layout;
  vkCreatePipelineLayout(device, &prepass_pipeline_layout_info, nullptr,
                         &m_prepass_pipeline_layout);

  VkDescriptorPoolSize prepass_pool_size{};
  prepass_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  prepass_pool_size.descriptorCount = 1;
  VkDescriptorPoolCreateInfo prepass_pool_info{};
  prepass_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  prepass_pool_info.poolSizeCount = 1;
  prepass_pool_info.pPoolSizes = &prepass_pool_size;
  prepass_pool_info.maxSets = 1;
  vkCreateDescriptorPool(device, &prepass_pool_info, nullptr,
                       &m_prepass_descriptor_pool);

  VkDescriptorSetAllocateInfo prepass_alloc_info{};
  prepass_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  prepass_alloc_info.descriptorPool = m_prepass_descriptor_pool;
  prepass_alloc_info.descriptorSetCount = 1;
  prepass_alloc_info.pSetLayouts = &m_prepass_descriptor_layout;
  vkAllocateDescriptorSets(device, &prepass_alloc_info, &m_prepass_descriptor_set);

  VkDescriptorBufferInfo prepass_ubo_info{};
  prepass_ubo_info.buffer = m_prepass_uniform_buffer->getBuffer();
  prepass_ubo_info.range = sizeof(OutlinePrepassUniformData);
  VkWriteDescriptorSet prepass_write{};
  prepass_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  prepass_write.dstSet = m_prepass_descriptor_set;
  prepass_write.dstBinding = 0;
  prepass_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  prepass_write.descriptorCount = 1;
  prepass_write.pBufferInfo = &prepass_ubo_info;
  vkUpdateDescriptorSets(device, 1, &prepass_write, 0, nullptr);

  VkDescriptorSetLayoutBinding resolve_bindings[5]{};
  resolve_bindings[0].binding = 0;
  resolve_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  resolve_bindings[0].descriptorCount = 1;
  resolve_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  for (uint32_t i = 1; i <= 4; ++i) {
    resolve_bindings[i].binding = i;
    resolve_bindings[i].descriptorCount = 1;
    resolve_bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    resolve_bindings[i].descriptorType =
        (i == 4) ? VK_DESCRIPTOR_TYPE_SAMPLER
                 : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  }

  VkDescriptorSetLayoutCreateInfo resolve_layout_info{};
  resolve_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  resolve_layout_info.bindingCount = 5;
  resolve_layout_info.pBindings = resolve_bindings;
  vkCreateDescriptorSetLayout(device, &resolve_layout_info, nullptr,
                              &m_resolve_descriptor_layout);

  VkPipelineLayoutCreateInfo resolve_pipeline_layout_info{};
  resolve_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  resolve_pipeline_layout_info.setLayoutCount = 1;
  resolve_pipeline_layout_info.pSetLayouts = &m_resolve_descriptor_layout;
  vkCreatePipelineLayout(device, &resolve_pipeline_layout_info, nullptr,
                         &m_resolve_pipeline_layout);

  VkDescriptorPoolSize resolve_pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
  };
  VkDescriptorPoolCreateInfo resolve_pool_info{};
  resolve_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  resolve_pool_info.poolSizeCount = 3;
  resolve_pool_info.pPoolSizes = resolve_pool_sizes;
  resolve_pool_info.maxSets = 1;
  vkCreateDescriptorPool(device, &resolve_pool_info, nullptr,
                       &m_resolve_descriptor_pool);

  VkDescriptorSetAllocateInfo resolve_alloc_info{};
  resolve_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  resolve_alloc_info.descriptorPool = m_resolve_descriptor_pool;
  resolve_alloc_info.descriptorSetCount = 1;
  resolve_alloc_info.pSetLayouts = &m_resolve_descriptor_layout;
  vkAllocateDescriptorSets(device, &resolve_alloc_info, &m_resolve_descriptor_set);
}

void OutlineOverlay::destroyDescriptorResources() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  m_prepass_descriptor_set = VK_NULL_HANDLE;
  m_resolve_descriptor_set = VK_NULL_HANDLE;
  if (m_prepass_descriptor_pool) {
    vkDestroyDescriptorPool(device, m_prepass_descriptor_pool, nullptr);
    m_prepass_descriptor_pool = VK_NULL_HANDLE;
  }
  if (m_resolve_descriptor_pool) {
    vkDestroyDescriptorPool(device, m_resolve_descriptor_pool, nullptr);
    m_resolve_descriptor_pool = VK_NULL_HANDLE;
  }
  if (m_prepass_pipeline_layout) {
    vkDestroyPipelineLayout(device, m_prepass_pipeline_layout, nullptr);
    m_prepass_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_resolve_pipeline_layout) {
    vkDestroyPipelineLayout(device, m_resolve_pipeline_layout, nullptr);
    m_resolve_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_prepass_descriptor_layout) {
    vkDestroyDescriptorSetLayout(device, m_prepass_descriptor_layout, nullptr);
    m_prepass_descriptor_layout = VK_NULL_HANDLE;
  }
  if (m_resolve_descriptor_layout) {
    vkDestroyDescriptorSetLayout(device, m_resolve_descriptor_layout, nullptr);
    m_resolve_descriptor_layout = VK_NULL_HANDLE;
  }
}

void OutlineOverlay::createResolveRenderPass() {
  VkAttachmentDescription color{};
  color.format = m_offscreen->getFormat();
  color.samples = VK_SAMPLE_COUNT_1_BIT;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depth{};
  depth.format = VK_FORMAT_D32_SFLOAT;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 1;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;
  subpass.pDepthStencilAttachment = &depth_ref;

  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask =
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkAttachmentDescription attachments[] = {color, depth};
  VkRenderPassCreateInfo rp_info{};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.attachmentCount = 2;
  rp_info.pAttachments = attachments;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;
  rp_info.dependencyCount = 2;
  rp_info.pDependencies = dependencies;
  vkCreateRenderPass(m_context->getDevice(), &rp_info, nullptr,
                     &m_resolve_render_pass);
}

void OutlineOverlay::destroyResolveRenderPass() {
  if (m_context && m_resolve_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_context->getDevice(), m_resolve_render_pass, nullptr);
    m_resolve_render_pass = VK_NULL_HANDLE;
  }
}

void OutlineOverlay::createPrepassPipeline() {
  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back(
      {"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  auto stages = VulkanShader::loadFromSlang(
      m_context->getDevice(), m_compiler, "engine/shaders/outline_prepass.slang",
      entries);
  m_prepass_pipeline = createGraphicsPipeline(
      m_context->getDevice(), m_targets->prepassRenderPass(),
      m_prepass_pipeline_layout, stages, false, true, true,
      VK_CULL_MODE_NONE);
  for (auto& stage : stages) {
    VulkanShader::destroyShaderModule(m_context->getDevice(), &stage.module);
  }
}

void OutlineOverlay::destroyPrepassPipeline() {
  if (m_context && m_prepass_pipeline) {
    vkDestroyPipeline(m_context->getDevice(), m_prepass_pipeline, nullptr);
    m_prepass_pipeline = VK_NULL_HANDLE;
  }
}

void OutlineOverlay::createResolvePipeline() {
  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back(
      {"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  auto stages = VulkanShader::loadFromSlang(
      m_context->getDevice(), m_compiler, "engine/shaders/outline_resolve.slang",
      entries);
  m_resolve_pipeline =
      createFullscreenPipeline(m_context->getDevice(), m_resolve_render_pass,
                               m_resolve_pipeline_layout, stages);
  for (auto& stage : stages) {
    VulkanShader::destroyShaderModule(m_context->getDevice(), &stage.module);
  }
}

void OutlineOverlay::destroyResolvePipeline() {
  if (m_context && m_resolve_pipeline) {
    vkDestroyPipeline(m_context->getDevice(), m_resolve_pipeline, nullptr);
    m_resolve_pipeline = VK_NULL_HANDLE;
  }
}

void OutlineOverlay::writeResolveDescriptors(OffscreenRenderTarget* offscreen) {
  if (!m_targets || !offscreen) {
    return;
  }
  VkDevice device = m_context->getDevice();

  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = m_resolve_uniform_buffer->getBuffer();
  ubo_info.range = sizeof(OutlineResolveUniformData);

  VkDescriptorImageInfo object_id_info{};
  object_id_info.imageView = m_targets->objectIdView();
  object_id_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo outline_depth_info{};
  outline_depth_info.imageView = m_targets->outlineDepthView();
  outline_depth_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo scene_depth_info{};
  scene_depth_info.imageView = offscreen->getDepthImageView();
  scene_depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo sampler_info{};
  sampler_info.sampler = m_depth_sampler;

  VkWriteDescriptorSet writes[5]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_resolve_descriptor_set;
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubo_info;

  VkDescriptorImageInfo* images[] = {&object_id_info, &outline_depth_info,
                                     &scene_depth_info};
  for (uint32_t i = 0; i < 3; ++i) {
    writes[i + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i + 1].dstSet = m_resolve_descriptor_set;
    writes[i + 1].dstBinding = i + 1;
    writes[i + 1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[i + 1].descriptorCount = 1;
    writes[i + 1].pImageInfo = images[i];
  }

  writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[4].dstSet = m_resolve_descriptor_set;
  writes[4].dstBinding = 4;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[4].descriptorCount = 1;
  writes[4].pImageInfo = &sampler_info;

  vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);
}

void OutlineOverlay::drawPrepass(VkCommandBuffer cmd, const OverlayState& state) {
  if (!enabled_ || m_cached_draws.empty() || m_prepass_pipeline == VK_NULL_HANDLE ||
      m_targets == nullptr) {
    return;
  }

  const VkExtent2D extent = m_targets->extent();
  if (extent.width == 0 || extent.height == 0) {
    return;
  }

  VkClearValue clears[2]{};
  clears[0].color.uint32[0] = 0;
  clears[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_targets->prepassRenderPass();
  begin.framebuffer = m_targets->prepassFramebuffer();
  begin.renderArea.extent = extent;
  begin.clearValueCount = 2;
  begin.pClearValues = clears;
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, extent};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_prepass_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_prepass_pipeline_layout, 0, 1,
                          &m_prepass_descriptor_set, 0, nullptr);

  for (const CachedDraw& draw : m_cached_draws) {
    if (draw.gpu_mesh == nullptr) {
      continue;
    }

    OutlinePrepassUniformData ubo{};
    ubo.model = draw.world_matrix;
    ubo.view = state.view;
    ubo.projection = state.projection;
    ubo.packed_object_id = static_cast<uint32_t>(draw.packed_id);
    m_prepass_uniform_buffer->upload(&ubo, sizeof(ubo));

    GpuMesh* mesh = draw.gpu_mesh;
    VkBuffer vertex_buffers[] = {mesh->getVertexBuffer()->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmd, mesh->getIndexBuffer()->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh->getIndexCount(), 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(cmd);
  m_targets->cmdBarrierToShaderRead(cmd);
}

void OutlineOverlay::drawResolve(VkCommandBuffer cmd,
                                 OffscreenRenderTarget* offscreen,
                                 const OverlayState& /*state*/) {
  if (!enabled_ || m_resolve_pipeline == VK_NULL_HANDLE || offscreen == nullptr ||
      m_targets == nullptr) {
    return;
  }

  const VkExtent2D extent = m_targets->extent();
  if (extent.width == 0 || extent.height == 0) {
    return;
  }

  offscreen->cmdBarrierDepthToShaderRead(cmd);
  writeResolveDescriptors(offscreen);

  OutlineResolveUniformData ubo{};
  ubo.screen_params = glm::vec4(
      1.0f / static_cast<float>(extent.width),
      1.0f / static_cast<float>(extent.height),
      static_cast<float>(extent.width), static_cast<float>(extent.height));
  m_resolve_uniform_buffer->upload(&ubo, sizeof(ubo));

  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_resolve_render_pass;
  begin.framebuffer = offscreen->getFramebuffer();
  begin.renderArea.extent = extent;
  begin.clearValueCount = 0;
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkViewport viewport{};
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, extent};

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolve_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_resolve_pipeline_layout, 0, 1,
                          &m_resolve_descriptor_set, 0, nullptr);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmd);

  offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  offscreen->setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

}  // namespace Blunder
