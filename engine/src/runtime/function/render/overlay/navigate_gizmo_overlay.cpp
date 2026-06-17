#include "runtime/function/render/overlay/navigate_gizmo_overlay.h"

#include <algorithm>
#include <cmath>
#include <vulkan/vulkan.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/function/render/overlay/navigate_gizmo_hit_test.h"
#include "runtime/function/render/overlay/navigate_gizmo_layout.h"
#include "runtime/function/render/overlay/navigate_gizmo_shared.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"

namespace Blunder {

namespace {

/// Must match GizmoUniformData in navigate_gizmo.slang.
struct GizmoUniformData {
  glm::mat4 view_rotation;
  glm::mat4 projection;
  glm::vec4 axis_color_x;
  glm::vec4 axis_color_y;
  glm::vec4 axis_color_z;
  glm::vec4 center_color;
  glm::vec4 bg_color;
  glm::vec4 params;
  glm::vec4 params2;
  glm::vec4 sort_order_0;
  glm::vec4 sort_order_1;
};

constexpr float k_neg_alpha_scale = 0.50f;
constexpr float k_letter_threshold = 0.14f;

constexpr uint32_t k_gizmo_vertex_count = 84u;

struct AxisSortEntry {
  float depth;
  int index;
};

}  // namespace

NavigateGizmoOverlay::~NavigateGizmoOverlay() {
  shutdown();
}

void NavigateGizmoOverlay::initialize(const OverlayResources& res,
                                      SlangCompiler* compiler) {
  ASSERT(res.vk_context);
  ASSERT(res.vk_allocator);
  ASSERT(compiler);
  ASSERT(res.screen_render_pass != VK_NULL_HANDLE);

  m_vk_context = res.vk_context;
  m_vk_allocator = res.vk_allocator;

  rhi::GraphicsPipelineDesc desc{};
  desc.shader_path = "engine/shaders/navigate_gizmo.slang";
  desc.enable_vertex_input = false;
  desc.topology = rhi::PrimitiveTopology::TriangleList;
  desc.cull_mode = rhi::CullMode::None;
  desc.enable_blend = true;
  // Screen HUD: must not depth-test against scene geometry loaded by ScreenOverlayPass.
  desc.enable_depth_test = false;
  desc.enable_depth_write = false;

  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(m_vk_context, compiler);
  m_pipeline->initializeWithRenderPass(res.screen_render_pass, desc);

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  m_uniform_buffers.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(GizmoUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

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
    LOG_FATAL("[NavigateGizmo] vkCreateDescriptorPool failed: {}",
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
    LOG_FATAL("[NavigateGizmo] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_descriptor_sets[i] = reinterpret_cast<uintptr_t>(sets[i]);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(GizmoUniformData);

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

void NavigateGizmoOverlay::shutdown() {
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
        device, reinterpret_cast<VkDescriptorPool>(m_descriptor_pool), nullptr);
    m_descriptor_pool = 0;
  }

  if (m_pipeline) {
    m_pipeline->shutdown();
    m_pipeline.reset();
  }

  m_vk_allocator = nullptr;
  m_vk_context = nullptr;
}

void NavigateGizmoOverlay::begin_sync(OverlayResources& /*res*/,
                                      const OverlayState& /*state*/) {
  enabled_ = true;
}

void NavigateGizmoOverlay::draw_screen(VkCommandBuffer cmd,
                                     const OverlayState& state) {
  if (!enabled_ || m_pipeline == nullptr) {
    return;
  }

  const NavigateGizmoLayout layout =
      computeNavigateGizmoLayout(state.viewport_width, state.viewport_height);
  if (!layout.visible) {
    return;
  }

  record_gizmo_draw(cmd, state, layout.gizmo_rect.x, layout.gizmo_rect.y,
                    layout.gizmo_size);
}

bool NavigateGizmoOverlay::tryHandleMouseClick(const Vec2& window_position,
                                               EditorCamera& camera) {
  if (!camera.isWindowPositionInViewport(window_position)) {
    return false;
  }

  const Vec2 viewport_local = camera.windowToViewportLocal(window_position);
  const uint32_t vp_w = static_cast<uint32_t>(camera.getViewportWidth());
  const uint32_t vp_h = static_cast<uint32_t>(camera.getViewportHeight());
  if (hitTestProjectionButtonViewportLocal(viewport_local.x, viewport_local.y,
                                           vp_w, vp_h)) {
    return true;
  }

  const NavigateGizmoLayout layout =
      computeNavigateGizmoLayout(vp_w, vp_h);
  if (!layout.visible) {
    return false;
  }

  const float proj_t = camera.getProjectionTransitionT();
  const float smooth_proj_t = proj_t * proj_t * (3.0f - 2.0f * proj_t);
  const float perspective_factor = 1.0f - smooth_proj_t;
  const std::optional<NavigateGizmoAxisHit> axis_hit =
      hitTestNavigateGizmoAxis(viewport_local.x, viewport_local.y, layout,
                               camera.getViewMatrix(), perspective_factor);
  if (!axis_hit.has_value()) {
    return false;
  }

  const int endpoint = axis_hit->endpoint_index;
  camera.alignToAxisView(static_cast<uint32_t>(endpoint / 2),
                         (endpoint % 2) == 0);
  return true;
}

void NavigateGizmoOverlay::record_gizmo_draw(VkCommandBuffer cmd,
                                             const OverlayState& state,
                                             float gizmo_x, float gizmo_y,
                                             float gizmo_size) {
  const float vw = static_cast<float>(state.viewport_width);
  const float vh = static_cast<float>(state.viewport_height);

  GizmoUniformData gizmo_ubo{};
  gizmo_ubo.view_rotation = state.view;
  gizmo_ubo.view_rotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  constexpr float k_D = 2.6556443f;
  const float t = state.projection_transition_t;
  const float smooth_t = t * t * (3.0f - 2.0f * t);

  glm::mat4 persp = glm::perspectiveZO(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);
  glm::mat4 ortho = glm::orthoZO(-1.1f, 1.1f, -1.1f, 1.1f, -10.0f, 10.0f);
  gizmo_ubo.projection = (1.0f - smooth_t) * persp + smooth_t * ortho;
  gizmo_ubo.projection[1][1] *= -1.0f;

  gizmo_ubo.axis_color_x = glm::vec4(kAxisColorPositiveX, 0.95f);
  gizmo_ubo.axis_color_y = glm::vec4(kAxisColorPositiveY, 0.95f);
  gizmo_ubo.axis_color_z = glm::vec4(kAxisColorPositiveZ, 0.95f);
  gizmo_ubo.center_color = glm::vec4(0.45f, 0.45f, 0.50f, 0.85f);
  gizmo_ubo.bg_color = glm::vec4(0.12f, 0.12f, 0.14f, 0.55f);
  gizmo_ubo.params = glm::vec4(
      NavigateGizmoMetrics::kArmLength, NavigateGizmoMetrics::kBallRadius,
      NavigateGizmoMetrics::kCenterRadius, NavigateGizmoMetrics::kNegBallRadius);
  gizmo_ubo.params2 =
      glm::vec4(NavigateGizmoMetrics::kArmHalfWidth, k_neg_alpha_scale,
                NavigateGizmoMetrics::kBgRadius, k_letter_threshold);

  AxisSortEntry entries[6];
  for (int i = 0; i < 3; ++i) {
    const float depth = state.view[i][2];
    entries[i * 2] = {depth, i * 2};
    entries[i * 2 + 1] = {-depth, i * 2 + 1};
  }
  std::sort(entries, entries + 6,
            [](const AxisSortEntry& a, const AxisSortEntry& b) {
              return a.depth < b.depth;
            });

  gizmo_ubo.sort_order_0 = glm::vec4(
      static_cast<float>(entries[0].index),
      static_cast<float>(entries[1].index),
      static_cast<float>(entries[2].index),
      static_cast<float>(entries[3].index));
  const float perspective_factor = 1.0f - smooth_t;
  gizmo_ubo.sort_order_1 = glm::vec4(
      static_cast<float>(entries[4].index),
      static_cast<float>(entries[5].index),
      k_D,
      perspective_factor);

  const uint32_t frame_index = state.frame_index;
  m_uniform_buffers[frame_index]->upload(&gizmo_ubo, sizeof(gizmo_ubo));

  VkViewport gizmo_viewport{};
  gizmo_viewport.x = gizmo_x;
  gizmo_viewport.y = gizmo_y;
  gizmo_viewport.width = gizmo_size;
  gizmo_viewport.height = gizmo_size;
  gizmo_viewport.minDepth = 0.0f;
  gizmo_viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &gizmo_viewport);

  VkRect2D gizmo_scissor{};
  gizmo_scissor.offset = {static_cast<int32_t>(gizmo_x),
                          static_cast<int32_t>(gizmo_y)};
  gizmo_scissor.extent = {static_cast<uint32_t>(gizmo_size),
                          static_cast<uint32_t>(gizmo_size)};
  vkCmdSetScissor(cmd, 0, 1, &gizmo_scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->nativePipeline()->getGraphicsPipeline());
  const VkDescriptorSet descriptor_set =
      reinterpret_cast<VkDescriptorSet>(m_descriptor_sets[frame_index]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->nativePipeline()->getPipelineLayout(),
                          0, 1, &descriptor_set, 0, nullptr);
  vkCmdDraw(cmd, k_gizmo_vertex_count, 1, 0, 0);

  VkViewport full_viewport{};
  full_viewport.width = vw;
  full_viewport.height = vh;
  full_viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &full_viewport);

  VkRect2D full_scissor{};
  full_scissor.extent = {state.viewport_width, state.viewport_height};
  vkCmdSetScissor(cmd, 0, 1, &full_scissor);
}

}  // namespace Blunder
