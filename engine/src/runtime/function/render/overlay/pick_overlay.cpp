#include "runtime/function/render/overlay/pick_overlay.h"

#include <slang.h>

#include <cmath>

#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/vec4.hpp>

#include "EASTL/unique_ptr.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/overlay/pick_targets.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

namespace {

struct PickPrepassUniformData {
  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 projection{1.0f};
  uint32_t entity_id{0};
  uint32_t alpha_mode{0};
  float alpha_cutoff{0.5f};
  float has_base_color_texture{0.0f};
  glm::vec4 base_color_factor{1.0f};
  uint32_t is_peel_pass{0};
  float peel_depth{0.0f};
  float peel_epsilon{1e-4f};
};

VkPipeline createGraphicsPipeline(VkDevice device, VkRenderPass render_pass,
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
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_TRUE;
  // Editor camera uses glm::perspectiveZO (near=1, far=0). GREATER + clear 0 picks
  // front-most; main mesh pass still uses LESS + clear 1 (see render-pipeline.md).
  depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER;

  VkPipelineColorBlendAttachmentState blend_attachment{};
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
  const VkResult result =
      vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[PickOverlay] vkCreateGraphicsPipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

VulkanTexture* resolveBaseColorTexture(RenderSystem& render_system,
                                       const MeshRendererComponent& renderer) {
  if (!renderer.material) {
    return render_system.getFallbackTexture();
  }
  const eastl::shared_ptr<Texture2DAsset>& texture_asset =
      renderer.material->getBaseColorTextureAsset();
  if (!texture_asset) {
    return render_system.getFallbackTexture();
  }
  VulkanTexture* uploaded = render_system.ensureTextureUploaded(texture_asset.get());
  return uploaded != nullptr ? uploaded : render_system.getFallbackTexture();
}

}  // namespace

PickOverlay::~PickOverlay() {
  shutdown();
}

void PickOverlay::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                             SlangCompiler* compiler, PickTargets* targets) {
  m_context = ctx;
  m_allocator = alloc;
  m_compiler = compiler;
  m_targets = targets;

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(ctx->getDevice(), &sampler_info, nullptr, &m_sampler);

  m_uniform_buffer = eastl::make_unique<VulkanBuffer>();
  m_uniform_buffer->create(alloc, sizeof(PickPrepassUniformData),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU);

  m_readback_buffer = eastl::make_unique<VulkanBuffer>();
  m_readback_buffer->create(alloc, sizeof(uint32_t),
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VMA_MEMORY_USAGE_GPU_TO_CPU);

  m_depth_readback_buffer = eastl::make_unique<VulkanBuffer>();
  m_depth_readback_buffer->create(alloc, sizeof(float),
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VMA_MEMORY_USAGE_GPU_TO_CPU);

  createDescriptorResources();
  createPipeline();
}

void PickOverlay::shutdown() {
  destroyPipeline();
  destroyDescriptorResources();

  if (m_context && m_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_context->getDevice(), m_sampler, nullptr);
    m_sampler = VK_NULL_HANDLE;
  }

  if (m_uniform_buffer) {
    m_uniform_buffer->destroy();
    m_uniform_buffer.reset();
  }
  if (m_readback_buffer) {
    m_readback_buffer->destroy();
    m_readback_buffer.reset();
  }
  if (m_depth_readback_buffer) {
    m_depth_readback_buffer->destroy();
    m_depth_readback_buffer.reset();
  }

  m_targets = nullptr;
  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
}

void PickOverlay::resize(uint32_t width, uint32_t height) {
  m_width = width;
  m_height = height;
}

void PickOverlay::createDescriptorResources() {
  VkDevice device = m_context->getDevice();

  VkDescriptorSetLayoutBinding bindings[3]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 3;
  layout_info.pBindings = bindings;
  vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &m_descriptor_layout);

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &m_descriptor_layout;
  vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                         &m_pipeline_layout);

  VkDescriptorPoolSize pool_sizes[2]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = 1;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = 1;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool);

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = m_descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &m_descriptor_layout;
  vkAllocateDescriptorSets(device, &alloc_info, &m_descriptor_set);

  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = m_uniform_buffer ? m_uniform_buffer->getBuffer() : VK_NULL_HANDLE;
  ubo_info.range = sizeof(PickPrepassUniformData);

  VkWriteDescriptorSet ubo_write{};
  ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  ubo_write.dstSet = m_descriptor_set;
  ubo_write.dstBinding = 0;
  ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_write.descriptorCount = 1;
  ubo_write.pBufferInfo = &ubo_info;
  vkUpdateDescriptorSets(device, 1, &ubo_write, 0, nullptr);
}

void PickOverlay::destroyDescriptorResources() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
    m_descriptor_set = VK_NULL_HANDLE;
  }
  if (m_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);
    m_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_descriptor_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_descriptor_layout, nullptr);
    m_descriptor_layout = VK_NULL_HANDLE;
  }
}

void PickOverlay::writeDescriptorTexture(VulkanTexture* texture) {
  if (m_context == nullptr || texture == nullptr) {
    return;
  }

  VkDescriptorImageInfo image_info{};
  image_info.imageView = texture->getImageView();
  image_info.sampler = m_sampler;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_descriptor_set;
  writes[0].dstBinding = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[0].descriptorCount = 1;
  writes[0].pImageInfo = &image_info;

  VkDescriptorImageInfo sampler_info{};
  sampler_info.sampler = m_sampler;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = m_descriptor_set;
  writes[1].dstBinding = 2;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[1].descriptorCount = 1;
  writes[1].pImageInfo = &sampler_info;

  vkUpdateDescriptorSets(m_context->getDevice(), 2, writes, 0, nullptr);
}

void PickOverlay::createPipeline() {
  if (m_targets == nullptr) {
    return;
  }

  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back(
      {"fragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  auto stages = VulkanShader::loadFromSlang(
      m_context->getDevice(), m_compiler, "engine/shaders/pick_prepass.slang",
      entries);
  m_pipeline = createGraphicsPipeline(m_context->getDevice(),
                                      m_targets->prepassRenderPass(),
                                      m_pipeline_layout, stages);
  for (auto& stage : stages) {
    VulkanShader::destroyShaderModule(m_context->getDevice(), &stage.module);
  }
}

void PickOverlay::destroyPipeline() {
  if (m_context && m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
    m_pipeline = VK_NULL_HANDLE;
  }
}

eastl::vector<PickOverlay::PickDraw> PickOverlay::collectPickableDraws(
    SceneInstance& scene, RenderSystem& render_system) {
  eastl::vector<PickDraw> draws;
  scene.forEachMeshRenderer([&](EntityId entity_id,
                                const MeshRendererComponent& renderer) {
    if (!renderer.mesh || renderer.alpha_mode == cgltf_alpha_mode_blend) {
      return;
    }

    GpuMesh* gpu_mesh = render_system.getOrUploadGpuMesh(renderer.mesh.get());
    if (gpu_mesh == nullptr || gpu_mesh->getVertexBuffer() == nullptr ||
        gpu_mesh->getIndexBuffer() == nullptr || gpu_mesh->getIndexCount() == 0) {
      return;
    }

    CachedDraw draw{};
    draw.gpu_mesh = gpu_mesh;
    draw.world_matrix = scene.getWorldMatrix(entity_id);
    draw.entity_id = entity_id;
    draw.alpha_mode = renderer.alpha_mode;
    draw.alpha_cutoff = renderer.alpha_cutoff;
    if (renderer.material) {
      draw.base_color_alpha = renderer.material->getBaseColorFactor().a;
      draw.has_base_color_texture = renderer.material->hasBaseColorTexture();
      draw.base_color_texture = resolveBaseColorTexture(render_system, renderer);
    } else {
      draw.base_color_texture = render_system.getFallbackTexture();
    }
    draws.push_back(draw);
  });
  return draws;
}

eastl::vector<PickOverlay::PickDraw> PickOverlay::filterPickDraws(
    const eastl::vector<PickDraw>& draws,
    const eastl::vector<EntityId>& excluded_entities) {
  if (excluded_entities.empty()) {
    return draws;
  }

  eastl::vector<PickDraw> filtered;
  filtered.reserve(draws.size());
  for (const PickDraw& draw : draws) {
    bool excluded = false;
    for (const EntityId entity_id : excluded_entities) {
      if (draw.entity_id == entity_id) {
        excluded = true;
        break;
      }
    }
    if (!excluded) {
      filtered.push_back(draw);
    }
  }
  return filtered;
}

eastl::vector<PickOverlay::PickDraw> PickOverlay::filterPickDrawsToEntities(
    const eastl::vector<PickDraw>& draws,
    const eastl::vector<EntityId>& include_entities) {
  if (include_entities.empty()) {
    return {};
  }

  eastl::vector<PickDraw> filtered;
  filtered.reserve(draws.size());
  for (const PickDraw& draw : draws) {
    for (const EntityId entity_id : include_entities) {
      if (draw.entity_id == entity_id) {
        filtered.push_back(draw);
        break;
      }
    }
  }
  return filtered;
}

bool PickOverlay::isPickSampleMiss(const EntityId entity_id, const float depth) {
  return !isValid(entity_id) ||
         depth <= k_pick_depth_clear_zo + k_peel_depth_epsilon;
}

void PickOverlay::drawPickPrepass(VkCommandBuffer cmd,
                                  const eastl::vector<CachedDraw>& draws,
                                  const glm::mat4& view, const glm::mat4& projection,
                                  RenderSystem& render_system, const float peel_depth,
                                  const bool is_peel_pass) {
  const VkExtent2D extent = m_targets->extent();

  VkClearValue clears[2]{};
  clears[0].color.uint32[0] = 0;
  clears[1].depthStencil = {k_pick_depth_clear_zo, 0};

  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_targets->prepassRenderPass();
  begin.framebuffer = m_targets->prepassFramebuffer();
  begin.renderArea.extent = extent;
  begin.clearValueCount = 2;
  begin.pClearValues = clears;
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
  m_targets->setEntityIdLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  m_targets->setPickDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  VkViewport viewport{};
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0}, extent};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0,
                          1, &m_descriptor_set, 0, nullptr);

  for (const CachedDraw& draw : draws) {
    PickPrepassUniformData ubo{};
    ubo.model = draw.world_matrix;
    ubo.view = view;
    ubo.projection = projection;
    ubo.entity_id = draw.entity_id;
    ubo.alpha_mode = draw.alpha_mode == cgltf_alpha_mode_mask ? 1u : 0u;
    ubo.alpha_cutoff = draw.alpha_cutoff;
    ubo.has_base_color_texture = draw.has_base_color_texture ? 1.0f : 0.0f;
    ubo.base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, draw.base_color_alpha);
    ubo.is_peel_pass = is_peel_pass ? 1u : 0u;
    ubo.peel_depth = peel_depth;
    ubo.peel_epsilon = k_peel_depth_epsilon;
    m_uniform_buffer->upload(&ubo, sizeof(ubo));
    writeDescriptorTexture(draw.base_color_texture);

    GpuMesh* mesh = draw.gpu_mesh;
    VkBuffer vertex_buffers[] = {mesh->getVertexBuffer()->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(cmd, mesh->getIndexBuffer()->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh->getIndexCount(), 1, 0, 0, 0);
  }

  vkCmdEndRenderPass(cmd);
}

bool PickOverlay::resolvePickPixel(const float window_x, const float window_y,
                                   EditorCamera& camera, int32_t& out_pixel_x,
                                   int32_t& out_pixel_y) const {
  if (m_targets == nullptr || m_pipeline == VK_NULL_HANDLE) {
    return false;
  }

  const VkExtent2D extent = m_targets->extent();
  if (extent.width == 0 || extent.height == 0) {
    return false;
  }

  const glm::ivec2 render_pixel =
      camera.windowToViewportRenderPixel(glm::vec2(window_x, window_y));
  out_pixel_x =
      glm::clamp(render_pixel.x, 0, static_cast<int32_t>(extent.width) - 1);
  out_pixel_y =
      glm::clamp(render_pixel.y, 0, static_cast<int32_t>(extent.height) - 1);
  return true;
}

void PickOverlay::recordPixelPickPass(
    const VkCommandBuffer cmd, const int32_t pixel_x, const int32_t pixel_y,
    const eastl::vector<PickDraw>& draws, const glm::mat4& view,
    const glm::mat4& projection, RenderSystem& render_system) {
  recordPixelPickPass(cmd, pixel_x, pixel_y, draws, view, projection,
                      render_system, 0.0f, false);
}

void PickOverlay::recordPixelPickPass(
    const VkCommandBuffer cmd, const int32_t pixel_x, const int32_t pixel_y,
    const eastl::vector<PickDraw>& draws, const glm::mat4& view,
    const glm::mat4& projection, RenderSystem& render_system,
    const float peel_depth, const bool is_peel_pass) {
  if (m_targets == nullptr || m_pipeline == VK_NULL_HANDLE ||
      m_uniform_buffer == nullptr || m_readback_buffer == nullptr ||
      m_depth_readback_buffer == nullptr || draws.empty()) {
    return;
  }

  m_targets->cmdPrepareForPrepassRender(cmd);
  drawPickPrepass(cmd, draws, view, projection, render_system, peel_depth,
                  is_peel_pass);
  m_targets->cmdBarrierEntityIdToTransferSrc(cmd);
  m_targets->cmdBarrierDepthToTransferSrc(cmd);

  VkBufferImageCopy id_region{};
  id_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  id_region.imageSubresource.layerCount = 1;
  id_region.imageOffset = {pixel_x, pixel_y, 0};
  id_region.imageExtent = {1, 1, 1};
  vkCmdCopyImageToBuffer(cmd, m_targets->entityIdImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         m_readback_buffer->getBuffer(), 1, &id_region);

  VkBufferImageCopy depth_region{};
  depth_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depth_region.imageSubresource.layerCount = 1;
  depth_region.imageOffset = {pixel_x, pixel_y, 0};
  depth_region.imageExtent = {1, 1, 1};
  vkCmdCopyImageToBuffer(cmd, m_targets->pickDepthImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         m_depth_readback_buffer->getBuffer(), 1, &depth_region);
}

PickOverlay::PickSample PickOverlay::readSubmittedPickSample() const {
  PickSample sample{};
  if (m_allocator == nullptr || m_readback_buffer == nullptr ||
      m_depth_readback_buffer == nullptr) {
    return sample;
  }

  void* id_mapped = nullptr;
  vmaMapMemory(m_allocator->getAllocator(), m_readback_buffer->getAllocation(),
               &id_mapped);
  const uint32_t picked_id = id_mapped ? *static_cast<const uint32_t*>(id_mapped) : 0u;
  vmaUnmapMemory(m_allocator->getAllocator(), m_readback_buffer->getAllocation());

  void* depth_mapped = nullptr;
  vmaMapMemory(m_allocator->getAllocator(),
               m_depth_readback_buffer->getAllocation(), &depth_mapped);
  const float picked_depth =
      depth_mapped ? *static_cast<const float*>(depth_mapped) : k_pick_depth_clear_zo;
  vmaUnmapMemory(m_allocator->getAllocator(),
                 m_depth_readback_buffer->getAllocation());

  if (isValid(static_cast<EntityId>(picked_id))) {
    sample.entity_id = static_cast<EntityId>(picked_id);
    sample.depth = picked_depth;
  }
  return sample;
}

PickOverlay::PickSample PickOverlay::samplePixelPickPass(
    const int32_t pixel_x, const int32_t pixel_y,
    const eastl::vector<CachedDraw>& draws, const glm::mat4& view,
    const glm::mat4& projection, RenderSystem& render_system) {
  return samplePixelPickPass(pixel_x, pixel_y, draws, view, projection,
                             render_system, 0.0f, false);
}

PickOverlay::PickSample PickOverlay::samplePixelPickPass(
    const int32_t pixel_x, const int32_t pixel_y,
    const eastl::vector<CachedDraw>& draws, const glm::mat4& view,
    const glm::mat4& projection, RenderSystem& render_system,
    const float peel_depth, const bool is_peel_pass) {
  PickSample sample{};
  if (m_targets == nullptr || m_pipeline == VK_NULL_HANDLE ||
      m_uniform_buffer == nullptr || m_readback_buffer == nullptr ||
      m_depth_readback_buffer == nullptr || draws.empty()) {
    return sample;
  }

  VkCommandBuffer cmd = m_context->beginImmediateCommands();
  recordPixelPickPass(cmd, pixel_x, pixel_y, draws, view, projection,
                      render_system, peel_depth, is_peel_pass);
  m_context->endImmediateCommands(cmd);
  return readSubmittedPickSample();
}

EntityId PickOverlay::pickAtWindowPosition(const float window_x,
                                           const float window_y,
                                           EditorCamera& camera,
                                           SceneInstance& scene,
                                           RenderSystem& render_system) {
  int32_t pixel_x = 0;
  int32_t pixel_y = 0;
  if (!resolvePickPixel(window_x, window_y, camera, pixel_x, pixel_y)) {
    return k_invalid_entity_id;
  }

  const eastl::vector<PickDraw> draws = collectPickableDraws(scene, render_system);
  if (draws.empty()) {
    return k_invalid_entity_id;
  }

  const glm::mat4 view = camera.getViewMatrix();
  const glm::mat4 projection = camera.getProjectionMatrix();
  const PickSample sample = samplePixelPickPass(pixel_x, pixel_y, draws, view,
                                                projection, render_system);
  return sample.entity_id;
}

eastl::vector<EntityId> PickOverlay::pickAllAtWindowPosition(
    const float window_x, const float window_y, EditorCamera& camera,
    SceneInstance& scene, RenderSystem& render_system) {
  eastl::vector<EntityId> hits;
  int32_t pixel_x = 0;
  int32_t pixel_y = 0;
  if (!resolvePickPixel(window_x, window_y, camera, pixel_x, pixel_y)) {
    return hits;
  }

  const eastl::vector<PickDraw> all_draws =
      collectPickableDraws(scene, render_system);
  if (all_draws.empty()) {
    return hits;
  }

  const glm::mat4 view = camera.getViewMatrix();
  const glm::mat4 projection = camera.getProjectionMatrix();

  eastl::vector<EntityId> excluded;
  for (int peel = 0; peel < k_max_peel_count; ++peel) {
    const eastl::vector<PickDraw> draws = filterPickDraws(all_draws, excluded);
    if (draws.empty()) {
      break;
    }

    const PickSample sample =
        samplePixelPickPass(pixel_x, pixel_y, draws, view, projection,
                            render_system);
    if (isPickSampleMiss(sample.entity_id, sample.depth)) {
      break;
    }

    hits.push_back(sample.entity_id);
    excluded.push_back(sample.entity_id);
  }

  return hits;
}

}  // namespace Blunder
