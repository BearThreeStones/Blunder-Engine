#include "runtime/function/render/overlay/grid_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vulkan/vulkan.h>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"

namespace Blunder {

namespace {

// Must match the shader constants.
constexpr uint32_t k_grid_steps_len = 8;
constexpr uint32_t k_grid_steps_draw = 3;

/// Must match GridUniformData in grid.slang.
struct GridUniformData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec4 camera_position_and_proj_type;
  glm::vec4 camera_forward_and_far_clip;
  glm::vec4 quad_center;
  glm::vec4 plane_axis_u;
  glm::vec4 plane_axis_v;
  glm::vec4 steps[k_grid_steps_len];
  // params: x=level, y=quadHalfExtent, z=alphaScale, w=pixelFac
  glm::vec4 params;
  glm::vec4 axis_color_u;
  glm::vec4 axis_color_v;
};

// Default step sizes (powers of 10, scene units).
constexpr float k_grid_default_steps[k_grid_steps_len] = {
    0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f};

constexpr float k_grid_base_alpha = 0.65f;
constexpr float k_grid_axis_alpha = 1.0f;

/// 6 vertices for a fullscreen quad (TriangleList).
constexpr uint32_t k_grid_vertex_count = 6u;

/// Compute the float `level` by finding where `dist` falls between
/// two adjacent step sizes.  Integer part = level index, fractional
/// part = smooth cross-fade progress.
float computeGridLevel(const float* steps, float dist) {
  for (int i = 0; i < static_cast<int>(k_grid_steps_len); ++i) {
    const float curr = steps[i];
    const float next = (i < static_cast<int>(k_grid_steps_len) - 1)
                           ? steps[i + 1]
                           : curr * 10.0f;
    if (next >= dist || i == static_cast<int>(k_grid_steps_len) - 1) {
      const float denom = next - curr;
      const float frac =
          (denom > 1e-8f) ? (dist - curr) / denom : 0.0f;
      return static_cast<float>(i) + std::clamp(frac, 0.0f, 1.0f);
    }
  }
  return 0.0f;
}

/// World point where the camera looks (orbit focal), projected onto the grid plane.
glm::vec3 gridFocalPointOnPlane(const OverlayState& state,
                                const glm::vec3& plane_normal) {
  const glm::vec3 focal =
      state.camera_position + state.camera_forward * state.camera_distance;
  return focal - glm::dot(focal, plane_normal) * plane_normal;
}

/// Unproject NDC (x,y,z) to world space.
glm::vec3 unprojectNdc(const glm::mat4& inv_vp, float ndc_x, float ndc_y,
                       float ndc_z) {
  glm::vec4 h = inv_vp * glm::vec4(ndc_x, ndc_y, ndc_z, 1.0f);
  if (std::abs(h.w) < 1e-8f) {
    return glm::vec3(0.0f);
  }
  return glm::vec3(h / h.w);
}

/// Cover the visible frustum intersection on the grid plane (centered on plane_center).
float computeGridHalfExtentOnPlane(const OverlayState& state,
                                   const glm::vec3& plane_normal,
                                   const glm::vec3& plane_center) {
  const bool is_ortho = std::abs(state.projection[3][3]) > 0.5f;
  const float aspect =
      static_cast<float>(state.viewport_width) /
      std::max(static_cast<float>(state.viewport_height), 1.0f);
  const float view_half_height =
      is_ortho ? state.ortho_size * 0.5f
               : std::tan(state.vertical_fov * 0.5f) * state.camera_distance;
  const float view_half_width = view_half_height * aspect;

  // Ortho: window diagonal on the plane; scale up when viewing the plane obliquely (Iso).
  if (is_ortho) {
    const float elev =
        std::abs(glm::dot(glm::normalize(state.camera_forward), plane_normal));
    const float oblique_scale = 1.0f / std::clamp(elev, 0.2f, 1.0f);
    const float from_window =
        std::hypot(view_half_width, view_half_height) * 2.5f * oblique_scale;
    const float extent_floor = state.ortho_size * 8.0f;
    // Iso logs: 61 world units was invisible; floor must dominate oblique window fit.
    return std::max(from_window, extent_floor);
  }

  const glm::mat4 inv_vp = glm::inverse(state.projection * state.view);
  const glm::vec3 cam = state.camera_position;
  float max_radius = 0.0f;
  constexpr float k_ndc_corners[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f},
                                         {1.0f, 1.0f}, {-1.0f, 1.0f}};
  for (const auto& corner : k_ndc_corners) {
    const glm::vec3 near_pt = unprojectNdc(inv_vp, corner[0], corner[1], 0.0f);
    const glm::vec3 far_pt = unprojectNdc(inv_vp, corner[0], corner[1], 1.0f);
    glm::vec3 dir = far_pt - near_pt;
    const float len = glm::length(dir);
    if (len < 1e-6f) {
      continue;
    }
    dir /= len;
    const float denom = glm::dot(dir, plane_normal);
    if (std::abs(denom) < 1e-6f) {
      continue;
    }
    const float t = glm::dot(plane_center - cam, plane_normal) / denom;
    if (t <= 0.0f) {
      continue;
    }
    const glm::vec3 hit = cam + dir * t;
    max_radius = std::max(max_radius, glm::length(hit - plane_center));
  }

  const float fallback = std::max(view_half_width, view_half_height) * 8.0f;
  return std::max(max_radius * 1.25f, fallback);
}

/// Compute a reference distance metric for the current camera.
float computeDistMetric(const OverlayState& state,
                        const glm::vec3& plane_normal) {
  (void)plane_normal;
  const bool is_ortho = std::abs(state.projection[3][3]) > 0.5f;
  const float vp_h =
      std::max(static_cast<float>(state.viewport_height), 1.0f);
  const float world_per_pixel =
      is_ortho ? (state.ortho_size * 2.0f / vp_h)
               : (2.0f * std::tan(state.vertical_fov * 0.5f) *
                  state.camera_distance / vp_h);
  // Target ~40 pixels per cell so grid lines are clearly spaced and visible.
  return std::max(world_per_pixel * 40.0f, 0.004f);
}

}  // namespace

GridOverlay::~GridOverlay() {
  shutdown();
}

void GridOverlay::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                             const OverlayResources& res,
                             SlangCompiler* compiler) {
  ASSERT(ctx);
  ASSERT(alloc);
  ASSERT(compiler);
  ASSERT(res.screen_render_pass != VK_NULL_HANDLE);

  m_vk_context = ctx;
  m_vk_allocator = alloc;

  rhi::GraphicsPipelineDesc desc{};
  desc.shader_path = "engine/shaders/grid.slang";
  desc.enable_vertex_input = false;
  desc.topology = rhi::PrimitiveTopology::TriangleList;
  desc.cull_mode = rhi::CullMode::None;
  desc.enable_blend = true;
  desc.enable_depth_test = false;
  desc.enable_depth_write = false;
  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(ctx, compiler);
  m_pipeline->initializeWithRenderPass(
      reinterpret_cast<VkRenderPass>(res.screen_render_pass), desc);

  // Per-frame uniform buffers.
  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  m_uniform_buffers.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(GridUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  // Descriptor pool and sets.
  VkDevice device = m_vk_context->getDevice();

  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = frames;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  pool_info.maxSets = frames;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  const VkResult pool_result =
      vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
  if (pool_result != VK_SUCCESS) {
    LOG_FATAL("[GridOverlay] vkCreateDescriptorPool failed: {}",
              static_cast<int>(pool_result));
  }
  m_descriptor_pool = reinterpret_cast<uintptr_t>(pool);

  const VkDescriptorSetLayout layout =
      m_pipeline->nativePipeline()->getDescriptorSetLayout();
  eastl::vector<VkDescriptorSetLayout> layouts(frames, layout);
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = frames;
  alloc_info.pSetLayouts = layouts.data();

  eastl::vector<VkDescriptorSet> sets(frames);
  const VkResult set_result =
      vkAllocateDescriptorSets(device, &alloc_info, sets.data());
  if (set_result != VK_SUCCESS) {
    LOG_FATAL("[GridOverlay] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_descriptor_sets[i] = reinterpret_cast<uintptr_t>(sets[i]);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(GridUniformData);

    VkWriteDescriptorSet ubo_write{};
    ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_write.dstSet = sets[i];
    ubo_write.dstBinding = 0;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.descriptorCount = 1;
    ubo_write.pBufferInfo = &buffer_info;
    vkUpdateDescriptorSets(device, 1, &ubo_write, 0, nullptr);
  }
}

void GridOverlay::shutdown() {
  if (m_vk_context == nullptr) {
    return;
  }

  VkDevice device = m_vk_context->getDevice();

  for (auto& buf : m_uniform_buffers) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_uniform_buffers.clear();

  m_descriptor_sets.clear();
  if (m_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        device, reinterpret_cast<VkDescriptorPool>(m_descriptor_pool),
        nullptr);
    m_descriptor_pool = 0;
  }

  if (m_pipeline) {
    m_pipeline->shutdown();
    m_pipeline.reset();
  }

  m_vk_allocator = nullptr;
  m_vk_context = nullptr;
}

void GridOverlay::begin_sync(OverlayResources& /*res*/,
                             const OverlayState& /*state*/) {
  enabled_ = true;
}

void GridOverlay::draw_screen(VkCommandBuffer cmd, const OverlayState& state) {
  if (!enabled_ || m_pipeline == nullptr) {
    return;
  }

  VkViewport viewport{};
  viewport.width = static_cast<float>(state.viewport_width);
  viewport.height = static_cast<float>(state.viewport_height);
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{{0, 0},
                   {state.viewport_width, state.viewport_height}};
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  const uint32_t frame_index = state.frame_index;

  GridUniformData grid_ubo{};
  grid_ubo.view = state.view;
  grid_ubo.projection = state.projection;

  // Match grid.slang: ortho iff projection[3][3] ~= 1 (see glm::ortho vs perspective).
  const bool is_ortho = std::abs(state.projection[3][3]) > 0.5f;
  grid_ubo.camera_position_and_proj_type =
      glm::vec4(state.camera_position, is_ortho ? 1.0f : 0.0f);
  grid_ubo.camera_forward_and_far_clip =
      glm::vec4(state.camera_forward, state.far_clip);

  // Set up grid plane axes and determine axis colors.
  glm::vec3 plane_normal;
  glm::vec4 axis_color_u;
  glm::vec4 axis_color_v;

  switch (state.grid_plane) {
    case ForwardGridPlane::xy:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      plane_normal = glm::vec3(0.0f, 0.0f, 1.0f);
      axis_color_u = glm::vec4(kAxisColorPositiveX, k_grid_axis_alpha);
      axis_color_v = glm::vec4(kAxisColorPositiveY, k_grid_axis_alpha);
      break;
    case ForwardGridPlane::yz:
      grid_ubo.plane_axis_u = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      plane_normal = glm::vec3(1.0f, 0.0f, 0.0f);
      axis_color_u = glm::vec4(kAxisColorPositiveY, k_grid_axis_alpha);
      axis_color_v = glm::vec4(kAxisColorPositiveZ, k_grid_axis_alpha);
      break;
    case ForwardGridPlane::xz:
    default:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      plane_normal = glm::vec3(0.0f, 1.0f, 0.0f);
      axis_color_u = glm::vec4(kAxisColorPositiveX, k_grid_axis_alpha);
      axis_color_v = glm::vec4(kAxisColorPositiveZ, k_grid_axis_alpha);
      break;
  }

  grid_ubo.axis_color_u = axis_color_u;
  grid_ubo.axis_color_v = axis_color_v;

  const float vp_h = std::max(static_cast<float>(state.viewport_height), 1.0f);
  const float world_per_pixel =
      is_ortho ? (state.ortho_size * 2.0f / vp_h)
               : (2.0f * std::tan(state.vertical_fov * 0.5f) *
                  state.camera_distance / vp_h);
  grid_ubo.plane_axis_u.w = world_per_pixel;

  // Fill steps array.
  for (uint32_t i = 0; i < k_grid_steps_len; ++i) {
    const float s = k_grid_default_steps[i];
    grid_ubo.steps[i] = glm::vec4(s, s, 0.0f, 0.0f);
  }

  // Compute reference distance and float level.
  const float dist = computeDistMetric(state, plane_normal);
  const float level = computeGridLevel(k_grid_default_steps, dist);

  // Center on look-at focal (orbit target), not camera ground footprint — Iso was missing
  // the grid at the origin because the quad followed the camera projection (G112).
  const glm::vec3 plane_center = gridFocalPointOnPlane(state, plane_normal);
  grid_ubo.quad_center = glm::vec4(plane_center, 1.0f);

  const float half_extent =
      computeGridHalfExtentOnPlane(state, plane_normal, plane_center);

  const float grid_alpha =
      is_ortho ? std::max(k_grid_base_alpha, 0.82f) : k_grid_base_alpha;
  grid_ubo.params =
      glm::vec4(level, half_extent, grid_alpha, state.camera_distance);

  // Upload uniform data.
  m_uniform_buffers[frame_index]->upload(&grid_ubo, sizeof(grid_ubo));

  // Bind pipeline and draw.
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->nativePipeline()->getGraphicsPipeline());
  const VkDescriptorSet descriptor_set =
      reinterpret_cast<VkDescriptorSet>(m_descriptor_sets[frame_index]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->nativePipeline()->getPipelineLayout(),
                          0, 1, &descriptor_set, 0, nullptr);
  vkCmdDraw(cmd, k_grid_vertex_count, 1, 0, 0);
}

}  // namespace Blunder
