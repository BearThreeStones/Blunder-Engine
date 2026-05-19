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
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/rhi_types.h"
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

struct GridUniformData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec4 camera_position_and_proj_type;
  glm::vec4 camera_forward_and_far_clip;
  glm::vec4 plane_origin;
  glm::vec4 plane_axis_u;
  glm::vec4 plane_axis_v;
  glm::vec4 params0;
  glm::vec4 params1;
  glm::vec4 params2;
};

constexpr float k_grid_major_scale = 10.0f;
constexpr float k_grid_half_extent = 120.0f;
constexpr float k_grid_base_alpha = 0.22f;
constexpr float k_grid_minor_alpha = 0.12f;
constexpr float k_grid_edge_fade = 1.45f;
constexpr float k_grid_angle_fade = 1.65f;
constexpr float k_grid_far_fade = 1.0f;
constexpr float k_grid_stipple_scale = 1.5f;
constexpr float k_grid_stipple_duty = 0.6f;
constexpr uint32_t k_grid_iterations = 3;
constexpr uint32_t k_grid_lines_per_axis = 96;

uint32_t opaqueDescriptorIndex(uint32_t slot_index, uint32_t frame_index) {
  return slot_index * VulkanSync::k_max_frames_in_flight + frame_index;
}

void writeOpaqueTextureBindings(VkDevice device, VkDescriptorSet descriptor_set,
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
  texture_writes[0].dstBinding = 1;
  texture_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  texture_writes[0].descriptorCount = 1;
  texture_writes[0].pImageInfo = &sampled_image_info;

  texture_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  texture_writes[1].dstSet = descriptor_set;
  texture_writes[1].dstBinding = 2;
  texture_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  texture_writes[1].descriptorCount = 1;
  texture_writes[1].pImageInfo = &sampler_info;

  vkUpdateDescriptorSets(device, 2, texture_writes, 0, nullptr);
}

void writeOpaqueShadowBinding(VkDevice device, VkDescriptorSet descriptor_set,
                              ShadowMapTarget* shadow_map) {
  if (shadow_map == nullptr) {
    return;
  }

  VkDescriptorImageInfo shadow_info{};
  shadow_info.imageView = shadow_map->getDepthImageView();
  shadow_info.sampler = shadow_map->getComparisonSampler();
  shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet shadow_write{};
  shadow_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  shadow_write.dstSet = descriptor_set;
  shadow_write.dstBinding = 3;
  shadow_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  shadow_write.descriptorCount = 1;
  shadow_write.pImageInfo = &shadow_info;
  vkUpdateDescriptorSets(device, 1, &shadow_write, 0, nullptr);
}

}  // namespace

ForwardRenderPath::~ForwardRenderPath() { shutdown(); }

void ForwardRenderPath::initialize(const ForwardRenderPathInit& init) {
  ASSERT(init.vk_context);
  ASSERT(init.vk_allocator);
  ASSERT(init.offscreen);
  ASSERT(init.grid_pipeline);
  ASSERT(init.opaque_pipeline);

  m_vk_context = init.vk_context;
  m_vk_allocator = init.vk_allocator;
  m_offscreen = init.offscreen;
  m_grid_pipeline = init.grid_pipeline;
  m_opaque_pipeline = init.opaque_pipeline;
  m_shadow_pipeline = init.shadow_pipeline;
  m_shadow_map = init.shadow_map;
  m_fallback_texture = init.fallback_texture;

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t opaque_slots = k_max_opaque_draws;
  const uint32_t total_opaque_sets = opaque_slots * frames;

  m_grid_uniform_buffers.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_grid_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_grid_uniform_buffers[i]->create(m_vk_allocator, sizeof(GridUniformData),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  m_opaque_uniform_buffers.resize(total_opaque_sets);
  for (uint32_t i = 0; i < total_opaque_sets; ++i) {
    m_opaque_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_opaque_uniform_buffers[i]->create(m_vk_allocator, sizeof(ForwardMeshUniformData),
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  m_shadow_uniform_buffers.resize(total_opaque_sets);
  for (uint32_t i = 0; i < total_opaque_sets; ++i) {
    m_shadow_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_shadow_uniform_buffers[i]->create(m_vk_allocator, sizeof(ShadowUniformData),
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  VkDevice device = m_vk_context->getDevice();

  VkDescriptorPoolSize opaque_pool_sizes[4]{};
  opaque_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  opaque_pool_sizes[0].descriptorCount = total_opaque_sets;
  opaque_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  opaque_pool_sizes[1].descriptorCount = total_opaque_sets;
  opaque_pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  opaque_pool_sizes[2].descriptorCount = total_opaque_sets;
  opaque_pool_sizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  opaque_pool_sizes[3].descriptorCount = total_opaque_sets;

  VkDescriptorPoolCreateInfo opaque_pool_info{};
  opaque_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  opaque_pool_info.poolSizeCount = 4;
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
      writeOpaqueTextureBindings(device, opaque_sets[i], m_fallback_texture);
    }
    if (m_shadow_map != nullptr) {
      writeOpaqueShadowBinding(device, opaque_sets[i], m_shadow_map);
    }
  }

  if (m_shadow_pipeline != nullptr) {
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

  VkDescriptorPoolSize grid_pool_size{};
  grid_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  grid_pool_size.descriptorCount = frames;

  VkDescriptorPoolCreateInfo grid_pool_info{};
  grid_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  grid_pool_info.poolSizeCount = 1;
  grid_pool_info.pPoolSizes = &grid_pool_size;
  grid_pool_info.maxSets = frames;
  VkDescriptorPool grid_pool = VK_NULL_HANDLE;
  const VkResult grid_pool_result =
      vkCreateDescriptorPool(device, &grid_pool_info, nullptr, &grid_pool);
  if (grid_pool_result != VK_SUCCESS) {
    LOG_FATAL("[ForwardRenderPath] grid vkCreateDescriptorPool failed: {}",
              static_cast<int>(grid_pool_result));
  }
  m_grid_descriptor_pool = reinterpret_cast<uintptr_t>(grid_pool);

  const VkDescriptorSetLayout grid_layout =
      m_grid_pipeline->nativePipeline()->getDescriptorSetLayout();
  eastl::vector<VkDescriptorSetLayout> grid_layouts(frames, grid_layout);
  VkDescriptorSetAllocateInfo grid_alloc_info{};
  grid_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  grid_alloc_info.descriptorPool = grid_pool;
  grid_alloc_info.descriptorSetCount = frames;
  grid_alloc_info.pSetLayouts = grid_layouts.data();

  eastl::vector<VkDescriptorSet> grid_sets(frames);
  const VkResult grid_set_result =
      vkAllocateDescriptorSets(device, &grid_alloc_info, grid_sets.data());
  if (grid_set_result != VK_SUCCESS) {
    LOG_FATAL("[ForwardRenderPath] grid vkAllocateDescriptorSets failed: {}",
              static_cast<int>(grid_set_result));
  }
  m_grid_descriptor_sets.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_grid_descriptor_sets[i] = reinterpret_cast<uintptr_t>(grid_sets[i]);

    VkDescriptorBufferInfo grid_buffer_info{};
    grid_buffer_info.buffer = m_grid_uniform_buffers[i]->getBuffer();
    grid_buffer_info.offset = 0;
    grid_buffer_info.range = sizeof(GridUniformData);

    VkWriteDescriptorSet grid_write{};
    grid_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    grid_write.dstSet = grid_sets[i];
    grid_write.dstBinding = 0;
    grid_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    grid_write.descriptorCount = 1;
    grid_write.pBufferInfo = &grid_buffer_info;
    vkUpdateDescriptorSets(device, 1, &grid_write, 0, nullptr);
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

  for (eastl::unique_ptr<VulkanBuffer>& buf : m_grid_uniform_buffers) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_grid_uniform_buffers.clear();

  m_opaque_descriptor_sets.clear();
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

  m_grid_descriptor_sets.clear();
  if (m_grid_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        device, reinterpret_cast<VkDescriptorPool>(m_grid_descriptor_pool),
        nullptr);
    m_grid_descriptor_pool = 0;
  }

  m_fallback_texture = nullptr;
  m_shadow_map = nullptr;
  m_shadow_pipeline = nullptr;
  m_opaque_pipeline = nullptr;
  m_grid_pipeline = nullptr;
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

void ForwardRenderPath::drawGrid(VkCommandBuffer cmd,
                                 const ForwardFrameState& frame_state,
                                 uint32_t frame_index) {
  GridUniformData grid_ubo{};
  grid_ubo.view = frame_state.view;
  grid_ubo.projection = frame_state.projection;
  grid_ubo.camera_position_and_proj_type = glm::vec4(
      frame_state.camera_position,
      frame_state.projection_mode == EditorCamera::ProjectionMode::orthographic
          ? 1.0f
          : 0.0f);
  grid_ubo.camera_forward_and_far_clip =
      glm::vec4(frame_state.camera_forward, frame_state.far_clip);

  switch (frame_state.grid_plane) {
    case ForwardGridPlane::xy:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      grid_ubo.plane_origin =
          glm::vec4(0.0f, 0.0f, frame_state.camera_position.z, 1.0f);
      break;
    case ForwardGridPlane::yz:
      grid_ubo.plane_axis_u = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      grid_ubo.plane_origin =
          glm::vec4(frame_state.camera_position.x, 0.0f, 0.0f, 1.0f);
      break;
    case ForwardGridPlane::xz:
    default:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      grid_ubo.plane_origin =
          glm::vec4(0.0f, frame_state.camera_position.y, 0.0f, 1.0f);
      break;
  }

  const float base_metric =
      frame_state.projection_mode == EditorCamera::ProjectionMode::orthographic
          ? frame_state.ortho_size
          : 2.0f * std::tan(frame_state.vertical_fov * 0.5f) *
                std::max(frame_state.camera_distance, frame_state.near_clip);
  const float step_power =
      std::floor(std::log10(std::max(base_metric * 0.1f, 0.0001f)));
  const float base_step = std::pow(10.0f, step_power);
  const float snapped_u =
      std::floor(glm::dot(glm::vec3(grid_ubo.plane_origin),
                          glm::vec3(grid_ubo.plane_axis_u)) /
                 base_step) *
      base_step;
  const float snapped_v =
      std::floor(glm::dot(glm::vec3(grid_ubo.plane_origin),
                          glm::vec3(grid_ubo.plane_axis_v)) /
                 base_step) *
      base_step;
  grid_ubo.plane_origin =
      snapped_u * grid_ubo.plane_axis_u + snapped_v * grid_ubo.plane_axis_v;
  grid_ubo.plane_origin.w = 1.0f;

  grid_ubo.params1 = glm::vec4(k_grid_edge_fade, k_grid_angle_fade, k_grid_far_fade,
                               k_grid_stipple_scale);
  const uint32_t vertex_count = k_grid_lines_per_axis * 4u;

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_grid_pipeline->nativePipeline()->getGraphicsPipeline());
  const VkDescriptorSet grid_descriptor_set = reinterpret_cast<VkDescriptorSet>(
      m_grid_descriptor_sets[frame_index]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_grid_pipeline->nativePipeline()->getPipelineLayout(), 0,
                          1, &grid_descriptor_set, 0, nullptr);

  for (uint32_t iteration = 0; iteration < k_grid_iterations; ++iteration) {
    const float scale =
        std::pow(k_grid_major_scale, static_cast<float>(iteration));
    const float step = base_step * scale;
    const float alpha_scale =
        iteration == 0 ? k_grid_base_alpha : k_grid_minor_alpha / scale;
    grid_ubo.params0 =
        glm::vec4(step, k_grid_major_scale, k_grid_half_extent * scale,
                  static_cast<float>(k_grid_lines_per_axis));
    grid_ubo.params2 = glm::vec4(k_grid_stipple_duty, alpha_scale,
                                 static_cast<float>(iteration), 0.0f);
    m_grid_uniform_buffers[frame_index]->upload(&grid_ubo, sizeof(grid_ubo));
    const float bias_scale = 1.0f + static_cast<float>(iteration) * 0.75f;
    vkCmdSetDepthBias(cmd, -1.2f * bias_scale, 0.0f, -1.2f * bias_scale);
    vkCmdDraw(cmd, vertex_count, 1, 0, 0);
  }
}

void ForwardRenderPath::drawOpaqueList(
    VkCommandBuffer cmd, const ForwardFrameState& frame_state,
    const ForwardOpaqueDraw* opaque_draws, uint32_t opaque_draw_count,
    uint32_t frame_index) {
  if (opaque_draws == nullptr || opaque_draw_count == 0) {
    return;
  }

  VkDevice device = m_vk_context->getDevice();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_opaque_pipeline->nativePipeline()->getGraphicsPipeline());

  for (uint32_t draw_i = 0; draw_i < opaque_draw_count; ++draw_i) {
    const ForwardOpaqueDraw& draw = opaque_draws[draw_i];
    if (draw.vertex_buffer == nullptr || draw.index_buffer == nullptr ||
        draw.index_count == 0 ||
        draw.slot_index >= k_max_opaque_draws) {
      continue;
    }

    const uint32_t descriptor_index =
        opaqueDescriptorIndex(draw.slot_index, frame_index);
    if (descriptor_index >= m_opaque_descriptor_sets.size()) {
      continue;
    }

    ForwardMeshUniformData mesh_ubo{};
    mesh_ubo.model = draw.model;
    mesh_ubo.normal_matrix = draw.normal_matrix;
    mesh_ubo.view = frame_state.view;
    mesh_ubo.projection = frame_state.projection;
    mesh_ubo.camera_position = glm::vec4(frame_state.camera_position, 1.0f);
    applyBlinnPhongToMeshUniforms(mesh_ubo, draw.material, frame_state.shading,
                                  frame_state);

    VulkanTexture* texture = draw.base_color_texture != nullptr
                                 ? draw.base_color_texture
                                 : m_fallback_texture;
    const VkDescriptorSet descriptor_set = reinterpret_cast<VkDescriptorSet>(
        m_opaque_descriptor_sets[descriptor_index]);
    if (texture != nullptr) {
      writeOpaqueTextureBindings(device, descriptor_set, texture);
    }

    m_opaque_uniform_buffers[descriptor_index]->upload(&mesh_ubo,
                                                       sizeof(mesh_ubo));

    VkBuffer vertex_buffers[] = {draw.vertex_buffer->getBuffer()};
    VkDeviceSize vertex_offsets[] = {0};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_opaque_pipeline->nativePipeline()->getPipelineLayout(),
                            0, 1, &descriptor_set, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, vertex_offsets);
    vkCmdBindIndexBuffer(cmd, draw.index_buffer->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, draw.index_count, 1, 0, 0, 0);
  }
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
  clears[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
  clears[1].depth_stencil = {1.0f, 0};

  m_offscreen->beginRenderPass(command_list, clears, 2);
  bindViewportScissor(command_buffer, extent.width, extent.height);
  drawGrid(command_buffer, frame_state, frame_index);
  drawOpaqueList(command_buffer, frame_state, opaque_draws, opaque_draw_count,
                 frame_index);
  m_offscreen->endRenderPass(command_list);
  m_offscreen->markPostRenderPassShaderRead();
}

}  // namespace Blunder
