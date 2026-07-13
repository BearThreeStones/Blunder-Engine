#include "runtime/function/render/forward/forward_render_path.h"

#include <cmath>
#include <vulkan/vulkan.h>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_opaque_draw.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/function/render/overlay/overlay_system.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/rhi_types.h"
#include "runtime/function/render/viewport_style.h"
#include "runtime/function/render/shadow/shadow_map_target.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"
#include "runtime/resource/asset/material_asset.h"

namespace Blunder {

namespace {

struct ShadowUniformData {
  glm::mat4 model;
  glm::mat4 light_view;
  glm::mat4 light_projection;
};

uint32_t opaqueDescriptorIndex(uint32_t slot_index, uint32_t frame_index) {
  return slot_index * VulkanSync::k_max_frames_in_flight + frame_index;
}

void writeTextureSamplerPair(VkDevice device, VkDescriptorSet descriptor_set,
                             uint32_t texture_binding, uint32_t sampler_binding,
                             VulkanTexture* texture) {
  if (texture == nullptr) {
    return;
  }

  VkDescriptorImageInfo sampled_image_info{};
  sampled_image_info.imageView = texture->getImageView();
  sampled_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo sampler_info{};
  sampler_info.sampler = texture->getSampler();

  VkWriteDescriptorSet texture_writes[2]{};
  texture_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  texture_writes[0].dstSet = descriptor_set;
  texture_writes[0].dstBinding = texture_binding;
  texture_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  texture_writes[0].descriptorCount = 1;
  texture_writes[0].pImageInfo = &sampled_image_info;

  texture_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  texture_writes[1].dstSet = descriptor_set;
  texture_writes[1].dstBinding = sampler_binding;
  texture_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  texture_writes[1].descriptorCount = 1;
  texture_writes[1].pImageInfo = &sampler_info;

  vkUpdateDescriptorSets(device, 2, texture_writes, 0, nullptr);
}

void writeOpaqueTextureBindings(VkDevice device, VkDescriptorSet descriptor_set,
                                VulkanTexture* base_color,
                                VulkanTexture* metallic_roughness,
                                VulkanTexture* normal_map,
                                VulkanTexture* occlusion,
                                VulkanTexture* fallback_texture) {
  if (fallback_texture == nullptr) {
    return;
  }
  writeTextureSamplerPair(device, descriptor_set, 1, 2,
                          base_color != nullptr ? base_color : fallback_texture);
  writeTextureSamplerPair(
      device, descriptor_set, 5, 6,
      metallic_roughness != nullptr ? metallic_roughness : fallback_texture);
  writeTextureSamplerPair(device, descriptor_set, 7, 8,
                          normal_map != nullptr ? normal_map : fallback_texture);
  writeTextureSamplerPair(device, descriptor_set, 9, 10,
                          occlusion != nullptr ? occlusion : fallback_texture);
}

void writeOpaqueShadowBinding(VkDevice device, VkDescriptorSet descriptor_set,
                              ShadowMapTarget* shadow_map) {
  if (shadow_map == nullptr) {
    return;
  }

  VkDescriptorImageInfo shadow_image_info{};
  shadow_image_info.imageView = shadow_map->getDepthImageView();
  shadow_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo shadow_sampler_info{};
  shadow_sampler_info.sampler = shadow_map->getComparisonSampler();

  VkWriteDescriptorSet shadow_writes[2]{};
  shadow_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  shadow_writes[0].dstSet = descriptor_set;
  shadow_writes[0].dstBinding = 3;
  shadow_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  shadow_writes[0].descriptorCount = 1;
  shadow_writes[0].pImageInfo = &shadow_image_info;

  shadow_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  shadow_writes[1].dstSet = descriptor_set;
  shadow_writes[1].dstBinding = 4;
  shadow_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  shadow_writes[1].descriptorCount = 1;
  shadow_writes[1].pImageInfo = &shadow_sampler_info;

  vkUpdateDescriptorSets(device, 2, shadow_writes, 0, nullptr);
}

}  // namespace

void ForwardRenderPath::updateOpaqueTextureBindingsIfNeeded(
    const uint32_t descriptor_index, VkDevice device,
    const VkDescriptorSet descriptor_set, VulkanTexture* base_color,
    VulkanTexture* metallic_roughness, VulkanTexture* normal_map,
    VulkanTexture* occlusion) {
  if (m_fallback_texture == nullptr ||
      descriptor_index >= m_opaque_texture_binding_cache.size()) {
    return;
  }

  VulkanTexture* const resolved_base =
      base_color != nullptr ? base_color : m_fallback_texture;
  VulkanTexture* const resolved_mr = metallic_roughness != nullptr
                                         ? metallic_roughness
                                         : m_fallback_texture;
  VulkanTexture* const resolved_normal =
      normal_map != nullptr ? normal_map : m_fallback_texture;
  VulkanTexture* const resolved_occlusion =
      occlusion != nullptr ? occlusion : m_fallback_texture;

  OpaqueTextureBindingCache& cache =
      m_opaque_texture_binding_cache[descriptor_index];
  if (cache.valid && cache.base_color == resolved_base &&
      cache.metallic_roughness == resolved_mr && cache.normal == resolved_normal &&
      cache.occlusion == resolved_occlusion) {
    return;
  }

  writeOpaqueTextureBindings(device, descriptor_set, resolved_base, resolved_mr,
                             resolved_normal, resolved_occlusion, m_fallback_texture);
  cache.base_color = resolved_base;
  cache.metallic_roughness = resolved_mr;
  cache.normal = resolved_normal;
  cache.occlusion = resolved_occlusion;
  cache.valid = true;
}

ForwardRenderPath::~ForwardRenderPath() { shutdown(); }

void ForwardRenderPath::initialize(const ForwardRenderPathInit& init) {
  ASSERT(init.vk_context);
  ASSERT(init.vk_allocator);
  ASSERT(init.offscreen);
  ASSERT(init.opaque_pipeline);

  m_vk_context = init.vk_context;
  m_vk_allocator = init.vk_allocator;
  m_offscreen = init.offscreen;
  m_opaque_pipeline = init.opaque_pipeline;
  m_shadow_pipeline = init.shadow_pipeline;
  m_transparent_pipeline = init.transparent_pipeline;
  m_shadow_map = init.shadow_map;
  m_fallback_texture = init.fallback_texture;
  m_overlay_system = init.overlay_system;

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t opaque_slots = k_max_opaque_draws;
  const uint32_t total_opaque_sets = opaque_slots * frames;

  m_opaque_uniform_buffers.resize(total_opaque_sets);
  for (uint32_t i = 0; i < total_opaque_sets; ++i) {
    m_opaque_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_opaque_uniform_buffers[i]->create(m_vk_allocator, sizeof(ForwardMeshUniformData),
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  VkDevice device = m_vk_context->getDevice();

  VkDescriptorPoolSize opaque_pool_sizes[3]{};
  opaque_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  opaque_pool_sizes[0].descriptorCount = total_opaque_sets;
  opaque_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  opaque_pool_sizes[1].descriptorCount = total_opaque_sets * 5u;
  opaque_pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  opaque_pool_sizes[2].descriptorCount = total_opaque_sets * 5u;

  VkDescriptorPoolCreateInfo opaque_pool_info{};
  opaque_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  opaque_pool_info.poolSizeCount = 3;
  opaque_pool_info.pPoolSizes = opaque_pool_sizes;
  opaque_pool_info.maxSets = total_opaque_sets;
  VkDescriptorPool opaque_pool = VK_NULL_HANDLE;
  const VkResult opaque_pool_result =
      vkCreateDescriptorPool(device, &opaque_pool_info, nullptr, &opaque_pool);
  if (opaque_pool_result != VK_SUCCESS) {
    LOG_FATAL(
        "[ForwardRenderPath] opaque vkCreateDescriptorPool failed: {}",
        static_cast<int>(opaque_pool_result));
  }
  m_opaque_descriptor_pool = reinterpret_cast<uintptr_t>(opaque_pool);

  const VkDescriptorSetLayout opaque_layout =
      m_opaque_pipeline->nativePipeline()->getDescriptorSetLayout();
  eastl::vector<VkDescriptorSetLayout> opaque_layouts(total_opaque_sets,
                                                      opaque_layout);
  VkDescriptorSetAllocateInfo opaque_alloc_info{};
  opaque_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  opaque_alloc_info.descriptorPool = opaque_pool;
  opaque_alloc_info.descriptorSetCount = total_opaque_sets;
  opaque_alloc_info.pSetLayouts = opaque_layouts.data();

  eastl::vector<VkDescriptorSet> opaque_sets(total_opaque_sets);
  const VkResult opaque_set_result = vkAllocateDescriptorSets(
      device, &opaque_alloc_info, opaque_sets.data());
  if (opaque_set_result != VK_SUCCESS) {
    LOG_FATAL(
        "[ForwardRenderPath] opaque vkAllocateDescriptorSets failed: {}",
        static_cast<int>(opaque_set_result));
  }
  m_opaque_descriptor_sets.resize(total_opaque_sets);
  m_opaque_texture_binding_cache.resize(total_opaque_sets);
  for (uint32_t i = 0; i < total_opaque_sets; ++i) {
    m_opaque_descriptor_sets[i] = reinterpret_cast<uintptr_t>(opaque_sets[i]);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_opaque_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(ForwardMeshUniformData);

    VkWriteDescriptorSet ubo_write{};
    ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_write.dstSet = opaque_sets[i];
    ubo_write.dstBinding = 0;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.descriptorCount = 1;
    ubo_write.pBufferInfo = &buffer_info;
    vkUpdateDescriptorSets(device, 1, &ubo_write, 0, nullptr);

    if (m_fallback_texture != nullptr) {
      writeOpaqueTextureBindings(device, opaque_sets[i], m_fallback_texture,
                                 m_fallback_texture, m_fallback_texture,
                                 m_fallback_texture, m_fallback_texture);
    }
    if (m_shadow_map != nullptr) {
      writeOpaqueShadowBinding(device, opaque_sets[i], m_shadow_map);
    }
  }

  if (m_shadow_pipeline != nullptr) {
    m_shadow_uniform_buffers.resize(total_opaque_sets);
    for (uint32_t i = 0; i < total_opaque_sets; ++i) {
      m_shadow_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
      m_shadow_uniform_buffers[i]->create(m_vk_allocator, sizeof(ShadowUniformData),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    VkDescriptorPoolSize shadow_pool_size{};
    shadow_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadow_pool_size.descriptorCount = total_opaque_sets;

    VkDescriptorPoolCreateInfo shadow_pool_info{};
    shadow_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    shadow_pool_info.poolSizeCount = 1;
    shadow_pool_info.pPoolSizes = &shadow_pool_size;
    shadow_pool_info.maxSets = total_opaque_sets;
    VkDescriptorPool shadow_pool = VK_NULL_HANDLE;
    const VkResult shadow_pool_result = vkCreateDescriptorPool(
        device, &shadow_pool_info, nullptr, &shadow_pool);
    if (shadow_pool_result != VK_SUCCESS) {
      LOG_FATAL("[ForwardRenderPath] shadow vkCreateDescriptorPool failed: {}",
                static_cast<int>(shadow_pool_result));
    }
    m_shadow_descriptor_pool = reinterpret_cast<uintptr_t>(shadow_pool);

    const VkDescriptorSetLayout shadow_layout =
        m_shadow_pipeline->nativePipeline()->getDescriptorSetLayout();
    eastl::vector<VkDescriptorSetLayout> shadow_layouts(total_opaque_sets,
                                                        shadow_layout);
    VkDescriptorSetAllocateInfo shadow_alloc_info{};
    shadow_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    shadow_alloc_info.descriptorPool = shadow_pool;
    shadow_alloc_info.descriptorSetCount = total_opaque_sets;
    shadow_alloc_info.pSetLayouts = shadow_layouts.data();

    eastl::vector<VkDescriptorSet> shadow_sets(total_opaque_sets);
    const VkResult shadow_set_result = vkAllocateDescriptorSets(
        device, &shadow_alloc_info, shadow_sets.data());
    if (shadow_set_result != VK_SUCCESS) {
      LOG_FATAL(
          "[ForwardRenderPath] shadow vkAllocateDescriptorSets failed: {}",
          static_cast<int>(shadow_set_result));
    }
    m_shadow_descriptor_sets.resize(total_opaque_sets);
    for (uint32_t i = 0; i < total_opaque_sets; ++i) {
      m_shadow_descriptor_sets[i] = reinterpret_cast<uintptr_t>(shadow_sets[i]);

      VkDescriptorBufferInfo buffer_info{};
      buffer_info.buffer = m_shadow_uniform_buffers[i]->getBuffer();
      buffer_info.offset = 0;
      buffer_info.range = sizeof(ShadowUniformData);

      VkWriteDescriptorSet ubo_write{};
      ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      ubo_write.dstSet = shadow_sets[i];
      ubo_write.dstBinding = 0;
      ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      ubo_write.descriptorCount = 1;
      ubo_write.pBufferInfo = &buffer_info;
      vkUpdateDescriptorSets(device, 1, &ubo_write, 0, nullptr);
    }
  }
}

void ForwardRenderPath::shutdown() {
  if (m_vk_context == nullptr) {
    return;
  }

  VkDevice device = m_vk_context->getDevice();

  for (eastl::unique_ptr<VulkanBuffer>& buf : m_opaque_uniform_buffers) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_opaque_uniform_buffers.clear();

  for (eastl::unique_ptr<VulkanBuffer>& buf : m_shadow_uniform_buffers) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_shadow_uniform_buffers.clear();

  m_opaque_descriptor_sets.clear();
  m_opaque_texture_binding_cache.clear();
  if (m_opaque_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        device, reinterpret_cast<VkDescriptorPool>(m_opaque_descriptor_pool),
        nullptr);
    m_opaque_descriptor_pool = 0;
  }

  m_shadow_descriptor_sets.clear();
  if (m_shadow_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        device, reinterpret_cast<VkDescriptorPool>(m_shadow_descriptor_pool),
        nullptr);
    m_shadow_descriptor_pool = 0;
  }

  m_overlay_system = nullptr;
  m_fallback_texture = nullptr;
  m_shadow_map = nullptr;
  m_shadow_pipeline = nullptr;
  m_transparent_pipeline = nullptr;
  m_opaque_pipeline = nullptr;
  m_offscreen = nullptr;
  m_vk_allocator = nullptr;
  m_vk_context = nullptr;
}

void ForwardRenderPath::bindViewportScissor(VkCommandBuffer cmd, uint32_t width,
                                            uint32_t height) {
  VkViewport viewport{};
  viewport.width = static_cast<float>(width);
  viewport.height = static_cast<float>(height);
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = {width, height};
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

namespace {

void drawMeshList(VkCommandBuffer cmd, VkDevice device,
                  vulkan_backend::VulkanGraphicsPipeline* pipeline,
                  const ForwardFrameState& frame_state,
                  const ForwardOpaqueDraw* draws, uint32_t draw_count,
                  uint32_t frame_index, VulkanTexture* fallback_texture,
                  const eastl::vector<uintptr_t>& descriptor_sets,
                  const eastl::vector<eastl::unique_ptr<VulkanBuffer>>& uniform_buffers,
                  ForwardRenderPath* render_path) {
  if (pipeline == nullptr || draws == nullptr || draw_count == 0) {
    return;
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->nativePipeline()->getGraphicsPipeline());

  for (uint32_t draw_i = 0; draw_i < draw_count; ++draw_i) {
    const ForwardOpaqueDraw& draw = draws[draw_i];
    if (draw.vertex_buffer == nullptr || draw.index_buffer == nullptr ||
        draw.index_count == 0 || draw.slot_index >= ForwardRenderPath::k_max_opaque_draws) {
      continue;
    }

    const uint32_t descriptor_index =
        opaqueDescriptorIndex(draw.slot_index, frame_index);
    if (descriptor_index >= descriptor_sets.size()) {
      continue;
    }

    ForwardMeshUniformData mesh_ubo{};
    mesh_ubo.model = draw.model;
    mesh_ubo.normal_matrix = draw.normal_matrix;
    mesh_ubo.view = frame_state.view;
    mesh_ubo.projection = frame_state.projection;
    mesh_ubo.camera_position = glm::vec4(frame_state.camera_position, 1.0f);
    applyPbrToMeshUniforms(mesh_ubo, draw.material, frame_state.shading, frame_state,
                             draw.alpha_mode, draw.alpha_cutoff, draw.double_sided);

    VulkanTexture* base_color =
        draw.base_color_texture != nullptr ? draw.base_color_texture : fallback_texture;
    VulkanTexture* metallic_roughness = draw.metallic_roughness_texture != nullptr
                                            ? draw.metallic_roughness_texture
                                            : fallback_texture;
    VulkanTexture* normal_map =
        draw.normal_texture != nullptr ? draw.normal_texture : fallback_texture;
    VulkanTexture* occlusion =
        draw.occlusion_texture != nullptr ? draw.occlusion_texture : fallback_texture;

    // Only enable PBR texture paths when a real map is bound (not the fallback).
    mesh_ubo.pbr_texture_flags.x =
        (draw.metallic_roughness_texture != nullptr &&
         draw.metallic_roughness_texture != fallback_texture)
            ? 1.0f
            : 0.0f;
    mesh_ubo.pbr_texture_flags.y =
        (draw.normal_texture != nullptr && draw.normal_texture != fallback_texture)
            ? 1.0f
            : 0.0f;
    mesh_ubo.pbr_texture_flags.z =
        (draw.occlusion_texture != nullptr && draw.occlusion_texture != fallback_texture)
            ? 1.0f
            : 0.0f;

    const VkDescriptorSet descriptor_set =
        reinterpret_cast<VkDescriptorSet>(descriptor_sets[descriptor_index]);
    if (render_path != nullptr) {
      render_path->updateOpaqueTextureBindingsIfNeeded(
          descriptor_index, device, descriptor_set, base_color, metallic_roughness,
          normal_map, occlusion);
    } else {
      writeOpaqueTextureBindings(device, descriptor_set, base_color,
                                 metallic_roughness, normal_map, occlusion,
                                 fallback_texture);
    }

    uniform_buffers[descriptor_index]->upload(&mesh_ubo, sizeof(mesh_ubo));

    VkBuffer vertex_buffers[] = {draw.vertex_buffer->getBuffer()};
    VkDeviceSize vertex_offsets[] = {0};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->nativePipeline()->getPipelineLayout(), 0, 1,
                            &descriptor_set, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, vertex_offsets);
    vkCmdBindIndexBuffer(cmd, draw.index_buffer->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, draw.index_count, 1, 0, 0, 0);
  }
}

}  // namespace

void ForwardRenderPath::drawOpaqueList(
    VkCommandBuffer cmd, const ForwardFrameState& frame_state,
    const ForwardOpaqueDraw* opaque_draws, uint32_t opaque_draw_count,
    uint32_t frame_index) {
  drawMeshList(cmd, m_vk_context->getDevice(), m_opaque_pipeline, frame_state,
               opaque_draws, opaque_draw_count, frame_index, m_fallback_texture,
               m_opaque_descriptor_sets, m_opaque_uniform_buffers, this);
}

void ForwardRenderPath::drawTransparentList(
    VkCommandBuffer cmd, const ForwardFrameState& frame_state,
    const ForwardOpaqueDraw* transparent_draws, uint32_t transparent_draw_count,
    uint32_t frame_index) {
  if (m_transparent_pipeline == nullptr) {
    return;
  }
  drawMeshList(cmd, m_vk_context->getDevice(), m_transparent_pipeline, frame_state,
               transparent_draws, transparent_draw_count, frame_index,
               m_fallback_texture, m_opaque_descriptor_sets, m_opaque_uniform_buffers,
               this);
}

void ForwardRenderPath::drawShadowOpaqueList(
    VkCommandBuffer cmd, const ForwardFrameState& frame_state,
    const ForwardOpaqueDraw* opaque_draws, uint32_t opaque_draw_count,
    uint32_t frame_index) {
  if (m_shadow_pipeline == nullptr || m_shadow_map == nullptr ||
      !frame_state.shadows_enabled || opaque_draws == nullptr ||
      opaque_draw_count == 0) {
    return;
  }

  const VkExtent2D shadow_extent = m_shadow_map->getExtent();
  bindViewportScissor(cmd, shadow_extent.width, shadow_extent.height);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_shadow_pipeline->nativePipeline()->getGraphicsPipeline());

  for (uint32_t draw_i = 0; draw_i < opaque_draw_count; ++draw_i) {
    const ForwardOpaqueDraw& draw = opaque_draws[draw_i];
    if (draw.vertex_buffer == nullptr || draw.index_buffer == nullptr ||
        draw.index_count == 0 || draw.slot_index >= k_max_opaque_draws) {
      continue;
    }

    const uint32_t descriptor_index =
        opaqueDescriptorIndex(draw.slot_index, frame_index);
    if (descriptor_index >= m_shadow_descriptor_sets.size()) {
      continue;
    }

    ShadowUniformData shadow_ubo{};
    shadow_ubo.model = draw.model;
    shadow_ubo.light_view = frame_state.light_view;
    shadow_ubo.light_projection = frame_state.light_projection;

    m_shadow_uniform_buffers[descriptor_index]->upload(&shadow_ubo,
                                                     sizeof(shadow_ubo));

    const VkDescriptorSet descriptor_set = reinterpret_cast<VkDescriptorSet>(
        m_shadow_descriptor_sets[descriptor_index]);

    VkBuffer vertex_buffers[] = {draw.vertex_buffer->getBuffer()};
    VkDeviceSize vertex_offsets[] = {0};
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_shadow_pipeline->nativePipeline()->getPipelineLayout(), 0, 1,
        &descriptor_set, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, vertex_offsets);
    vkCmdBindIndexBuffer(cmd, draw.index_buffer->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, draw.index_count, 1, 0, 0, 0);
  }
}

void ForwardRenderPath::renderFrame(VkCommandBuffer command_buffer,
                                    const ForwardFrameState& frame_state,
                                    const ForwardOpaqueDraw* opaque_draws,
                                    uint32_t opaque_draw_count,
                                    const ForwardOpaqueDraw* transparent_draws,
                                    uint32_t transparent_draw_count,
                                    uint32_t frame_index) {
  ASSERT(m_offscreen);
  const rhi::Extent2D extent = m_offscreen->extent();
  if (extent.width == 0 || extent.height == 0) {
    return;
  }

  if (m_shadow_map != nullptr && frame_state.shadows_enabled) {
    m_shadow_map->beginRenderPass(command_buffer);
    drawShadowOpaqueList(command_buffer, frame_state, opaque_draws,
                         opaque_draw_count, frame_index);
    m_shadow_map->endRenderPass(command_buffer);
    m_shadow_map->cmdBarrierToShaderReadDepth(command_buffer);
  }

  vulkan_backend::VulkanCommandList command_list;
  command_list.bind(m_vk_context, command_buffer);

  rhi::ClearValue clears[2]{};
  clears[0].color = {kViewportBackgroundRgb, kViewportBackgroundRgb,
                     kViewportBackgroundRgb, 1.0f};
  clears[1].depth_stencil = {1.0f, 0};

  m_offscreen->beginRenderPass(command_list, clears, 2);
  bindViewportScissor(command_buffer, extent.width, extent.height);

  // Scene passes: opaque (depth write ON), then transparent (depth write OFF).
  drawOpaqueList(command_buffer, frame_state, opaque_draws, opaque_draw_count,
                 frame_index);
  drawTransparentList(command_buffer, frame_state, transparent_draws,
                      transparent_draw_count, frame_index);

  // Overlay pass: draws grid, axes, wireframes, etc. after scene geometry.
  // Overlays read the scene depth but do not write to it.
  if (m_overlay_system != nullptr) {
    m_overlay_system->draw_scene_overlays(command_buffer);
  }

  m_offscreen->endRenderPass(command_list);
  m_offscreen->markPostRenderPassShaderRead();
}

}  // namespace Blunder
