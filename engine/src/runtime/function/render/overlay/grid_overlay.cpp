#include "runtime/function/render/overlay/grid_overlay.h"

#include <cmath>
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

struct GridUniformData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec4 camera_position_and_proj_type;
  glm::vec4 camera_forward_and_far_clip;
  glm::vec4 plane_origin;
  glm::vec4 plane_axis_u;
  glm::vec4 plane_axis_v;
  glm::vec4 steps[k_grid_steps_len];  // per-level step sizes: [i].xy = stepU, stepV
  // params: x=level(float), y=numLinesPerLevel, z=alphaScale, w=pixelFac
  glm::vec4 params;
  glm::vec4 axis_color_x;
  glm::vec4 axis_color_y;
  glm::vec4 axis_color_z;
};

// Default step sizes (powers of 10, scene units).
// steps[0] = 0.01, steps[1] = 0.1, steps[2] = 1.0, ...
constexpr float k_grid_default_steps[k_grid_steps_len] = {
    0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f};

constexpr float k_grid_base_alpha = 0.28f;
constexpr uint32_t k_grid_lines_per_axis = 96;
constexpr uint32_t k_grid_axis_line_vertices = 6u;
constexpr float k_grid_axis_alpha = 0.95f;

/// Compute the float `level` by iterating through the steps array to find
/// where `dist` falls between two adjacent step sizes.  The integer part
/// is the level index and the fractional part drives smooth cross-fade.
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

/// Compute a reference distance metric for the current camera.
/// Blender: interpolates between "directly below" and "looking at" distances
/// for perspective; uses pixel-derived scale for ortho.
float computeDistMetric(const OverlayState& state, const glm::vec3& plane_normal) {
  if (state.is_perspective) {
    // Distance from camera to the grid plane along the view direction.
    const float height = std::abs(glm::dot(state.camera_position, plane_normal));
    const float cos_angle = std::abs(glm::dot(state.camera_forward, plane_normal));
    // Interpolate between looking straight down (height) and along view dir
    // (height / cos), weighted by cos to avoid division by zero at grazing.
    const float view_dist =
        (cos_angle > 1e-4f) ? height / cos_angle : height * 1000.0f;
    // Blend: at steep angles use direct height, at shallow use view distance.
    return view_dist * (1.0f - cos_angle) + height * cos_angle;
  }
  // Orthographic: scale from ortho size.
  return state.ortho_size;
}

}  // namespace

GridOverlay::~GridOverlay() {
  shutdown();
}

void GridOverlay::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                             rhi::IOffscreenRenderTarget* offscreen,
                             SlangCompiler* compiler) {
  ASSERT(ctx);
  ASSERT(alloc);
  ASSERT(offscreen);
  ASSERT(compiler);

  m_vk_context = ctx;
  m_vk_allocator = alloc;

  // Create the grid pipeline (self-owned).
  rhi::GraphicsPipelineDesc grid_pipeline_desc{};
  grid_pipeline_desc.shader_path = "engine/shaders/grid.slang";
  grid_pipeline_desc.enable_vertex_input = false;
  grid_pipeline_desc.topology = rhi::PrimitiveTopology::LineList;
  grid_pipeline_desc.cull_mode = rhi::CullMode::None;
  grid_pipeline_desc.enable_blend = true;
  grid_pipeline_desc.enable_depth_test = true;
  grid_pipeline_desc.enable_depth_write = false;
  grid_pipeline_desc.enable_depth_bias = true;
  grid_pipeline_desc.depth_bias_constant_factor = -1.2f;
  grid_pipeline_desc.depth_bias_slope_factor = -1.2f;
  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(ctx, compiler);
  m_pipeline->initialize(*offscreen, grid_pipeline_desc);

  // Allocate per-frame uniform buffers.
  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  m_uniform_buffers.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(GridUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  // Create descriptor pool and sets.
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

void GridOverlay::draw_line(VkCommandBuffer cmd,
                            const OverlayState& state) {
  if (!enabled_ || m_pipeline == nullptr) {
    return;
  }

  const uint32_t frame_index = state.frame_index;

  GridUniformData grid_ubo{};
  grid_ubo.view = state.view;
  grid_ubo.projection = state.projection;

  const bool is_ortho = !state.is_perspective;
  grid_ubo.camera_position_and_proj_type =
      glm::vec4(state.camera_position, is_ortho ? 1.0f : 0.0f);
  grid_ubo.camera_forward_and_far_clip =
      glm::vec4(state.camera_forward, state.far_clip);

  // Set up grid plane.
  glm::vec3 plane_normal;
  switch (state.grid_plane) {
    case ForwardGridPlane::xy:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      grid_ubo.plane_origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      plane_normal = glm::vec3(0.0f, 0.0f, 1.0f);
      break;
    case ForwardGridPlane::yz:
      grid_ubo.plane_axis_u = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      grid_ubo.plane_origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      plane_normal = glm::vec3(1.0f, 0.0f, 0.0f);
      break;
    case ForwardGridPlane::xz:
    default:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      grid_ubo.plane_origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      plane_normal = glm::vec3(0.0f, 1.0f, 0.0f);
      break;
  }

  // Fill steps array.
  for (uint32_t i = 0; i < k_grid_steps_len; ++i) {
    const float s = k_grid_default_steps[i];
    grid_ubo.steps[i] = glm::vec4(s, s, 0.0f, 0.0f);
  }

  // Compute reference distance and float level.
  const float dist = computeDistMetric(state, plane_normal);
  const float level = computeGridLevel(k_grid_default_steps, dist);

  // Store camera offset on plane_origin so the shader can snap per-level.
  const glm::vec3 axis_u = glm::vec3(grid_ubo.plane_axis_u);
  const glm::vec3 axis_v = glm::vec3(grid_ubo.plane_axis_v);
  const float cam_u = glm::dot(state.camera_position, axis_u);
  const float cam_v = glm::dot(state.camera_position, axis_v);
  grid_ubo.plane_origin =
      glm::vec4(cam_u * axis_u + cam_v * axis_v, 1.0f);

  // Pixel size factor for orthographic fade.
  const float pixel_fac =
      is_ortho
          ? (state.ortho_size * 2.0f /
             std::max(static_cast<float>(state.viewport_height), 1.0f))
          : 0.0f;

  // Vertices per level: linesPerAxis lines × 2 axes × 2 vertices/line.
  const uint32_t num_lines_per_level = k_grid_lines_per_axis * 4u;
  const uint32_t total_grid_vertices = k_grid_steps_draw * num_lines_per_level;

  grid_ubo.params = glm::vec4(
      level,
      static_cast<float>(num_lines_per_level),
      k_grid_base_alpha,
      pixel_fac);
  grid_ubo.axis_color_x = glm::vec4(kAxisColorPositiveX, k_grid_axis_alpha);
  grid_ubo.axis_color_y = glm::vec4(kAxisColorPositiveY, k_grid_axis_alpha);
  grid_ubo.axis_color_z = glm::vec4(kAxisColorPositiveZ, k_grid_axis_alpha);

  // Upload uniform data.
  m_uniform_buffers[frame_index]->upload(&grid_ubo, sizeof(grid_ubo));

  // Bind pipeline and descriptors.
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->nativePipeline()->getGraphicsPipeline());
  const VkDescriptorSet descriptor_set = reinterpret_cast<VkDescriptorSet>(
      m_descriptor_sets[frame_index]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->nativePipeline()->getPipelineLayout(),
                          0, 1, &descriptor_set, 0, nullptr);

  // Single draw call: 3 levels of grid + 3 axis lines.
  vkCmdSetDepthBias(cmd, -1.2f, 0.0f, -1.2f);
  vkCmdDraw(cmd, total_grid_vertices + k_grid_axis_line_vertices, 1, 0, 0);
}

}  // namespace Blunder
