#include "runtime/function/render/gizmo/transform_gizmo_overlay.h"

#include <algorithm>
#include <cstdio>
#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/gizmo/transform_gizmo_shared.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
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

/// Max handles drawn per frame (translate plane drag: live handles + guides + ghost + dot).
constexpr uint32_t k_max_gizmo_draws_per_frame = 12u;

constexpr glm::vec4 k_plane_quad_layout{0.2f, 0.2f, 0.1f, 0.1f};
constexpr float k_plane_quad_z = 0.05f;
constexpr glm::vec4 k_center_quad_layout{0.0f, 0.0f, 0.22f, 0.22f};
constexpr glm::vec4 k_scale_tip_quad_layout{0.0f, 0.0f, 0.14f, 0.14f};
constexpr float k_scale_tip_quad_z = 0.8f;
constexpr float k_scale_center_quad_z = 0.05f;
constexpr float k_translate_guide_scale_multiplier = 18.0f;
constexpr float k_translate_guide_alpha = 0.35f;
constexpr float k_translate_ghost_alpha = 0.5f;
constexpr float k_translate_origin_dot_scale = 0.28f;

TransformGizmoUniformData makeUniform(const OverlayState& state,
                                      const glm::mat4& gizmo_world,
                                      const glm::vec4& color, GizmoDrawStyle style,
                                      float alpha, const glm::vec4& quad_layout,
                                      float quad_z, float line_width_scale = 1.0f,
                                      float arc_start = 0.0f, float arc_delta = 0.0f) {
  TransformGizmoUniformData u{};
  u.view = state.view;
  u.proj = state.projection;
  u.gizmo_world = gizmo_world;
  u.color = color;
  const float params_z =
      style == GizmoDrawStyle::dial_ghost ? arc_start : line_width_scale;
  u.params = glm::vec4(static_cast<float>(static_cast<uint32_t>(style)), alpha, params_z,
                       arc_delta);
  u.quad_layout = quad_layout;
  u.quad_z = quad_z;
  return u;
}

GizmoDrawStyle styleForTranslateAxis(ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::trans_xy:
    case ManipulatorAxis::trans_yz:
    case ManipulatorAxis::trans_zx:
    case ManipulatorAxis::trans_c:
      return GizmoDrawStyle::plane;
    default:
      return GizmoDrawStyle::arrow;
  }
}

glm::vec3 translatePlaneHandleCenter(const GizmoBasis& basis,
                                     const ManipulatorAxis plane,
                                     const float handle_scale) {
  const float offset =
      handle_scale * TransformGizmoMetrics::k_mesh_plane_center_offset;
  switch (plane) {
    case ManipulatorAxis::trans_xy:
      return basis.origin + (basis.axis_x + basis.axis_y) * offset;
    case ManipulatorAxis::trans_yz:
      return basis.origin + (basis.axis_y + basis.axis_z) * offset;
    case ManipulatorAxis::trans_zx:
      return basis.origin + (basis.axis_z + basis.axis_x) * offset;
    default:
      return basis.origin;
  }
}

uint32_t vertexCountForStyle(GizmoDrawStyle style) {
  switch (style) {
    case GizmoDrawStyle::plane:
      return TransformGizmoDrawCounts::k_plane_verts;
    case GizmoDrawStyle::center:
      return TransformGizmoDrawCounts::k_center_verts;
    case GizmoDrawStyle::dial:
    case GizmoDrawStyle::dial_ghost:
      return TransformGizmoDrawCounts::k_dial_verts;
    case GizmoDrawStyle::scale_box:
      return TransformGizmoDrawCounts::k_scale_box_verts;
    case GizmoDrawStyle::arrow:
      return TransformGizmoDrawCounts::k_arrow_verts;
    default:
      return TransformGizmoDrawCounts::k_arrow_verts;
  }
}

}  // namespace

TransformGizmoOverlay::~TransformGizmoOverlay() {
  shutdown();
}

void TransformGizmoOverlay::initialize(const OverlayResources& res,
                                       SlangCompiler* compiler) {
  ASSERT(res.vk_context);
  ASSERT(res.vk_allocator);
  ASSERT(compiler);
  ASSERT(res.screen_render_pass != VK_NULL_HANDLE);

  m_vk_context = res.vk_context;
  m_vk_allocator = res.vk_allocator;

  rhi::GraphicsPipelineDesc desc{};
  desc.shader_path = "engine/shaders/transform_gizmo.slang";
  desc.enable_vertex_input = false;
  desc.topology = rhi::PrimitiveTopology::TriangleList;
  desc.cull_mode = rhi::CullMode::None;
  desc.enable_blend = true;
  // ScreenOverlayPass runs after SSAO; must not depth-test scene geometry.
  desc.enable_depth_test = false;
  desc.enable_depth_write = false;

  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(m_vk_context, compiler);
  m_pipeline->initializeWithRenderPass(res.screen_render_pass, desc);

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t total_sets = frames * k_max_gizmo_draws_per_frame;
  m_uniform_buffers.resize(total_sets);
  for (uint32_t i = 0; i < total_sets; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(TransformGizmoUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  VkDevice device = m_vk_context->getDevice();
  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = total_sets;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  pool_info.maxSets = total_sets;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  const VkResult pool_result =
      vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
  if (pool_result != VK_SUCCESS) {
    LOG_FATAL("[TransformGizmo] vkCreateDescriptorPool failed: {}",
              static_cast<int>(pool_result));
  }
  m_descriptor_pool = reinterpret_cast<uintptr_t>(pool);

  const VkDescriptorSetLayout layout =
      m_pipeline->nativePipeline()->getDescriptorSetLayout();
  eastl::vector<VkDescriptorSetLayout> layouts(total_sets, layout);
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = total_sets;
  alloc_info.pSetLayouts = layouts.data();

  eastl::vector<VkDescriptorSet> sets(total_sets);
  const VkResult set_result =
      vkAllocateDescriptorSets(device, &alloc_info, sets.data());
  if (set_result != VK_SUCCESS) {
    LOG_FATAL("[TransformGizmo] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(total_sets);
  for (uint32_t i = 0; i < total_sets; ++i) {
    m_descriptor_sets[i] = reinterpret_cast<uintptr_t>(sets[i]);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(TransformGizmoUniformData);

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

void TransformGizmoOverlay::shutdown() {
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

void TransformGizmoOverlay::begin_sync(OverlayResources& /*res*/,
                                       const OverlayState& state) {
  const bool grab_translate_session =
      m_controller.isTranslateModalSessionActive() &&
      m_controller.translateModalSession().isGrabEntry();
  enabled_ = state.has_selection &&
             (state.gizmo_mode == TransformGizmoMode::translate ||
              state.gizmo_mode == TransformGizmoMode::rotate ||
              state.gizmo_mode == TransformGizmoMode::scale ||
              grab_translate_session);
}

void TransformGizmoOverlay::draw_screen(VkCommandBuffer cmd,
                                        const OverlayState& state) {
  if (!enabled_ || m_pipeline == nullptr || !state.has_selection) {
    return;
  }

  recordGizmoDraw(cmd, state);
}

void TransformGizmoOverlay::recordGizmoDraw(VkCommandBuffer cmd,
                                            const OverlayState& state) {
  m_next_draw_slot = 0;
  GizmoBasis basis{};
  if (!m_controller.buildActiveGizmoBasis(basis)) {
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

  TransformGizmoScaleContext scale_ctx{};
  scale_ctx.view_projection = state.projection * state.view;
  scale_ctx.pivot = basis.origin;
  scale_ctx.viewport_height = static_cast<float>(state.viewport_height);
  scale_ctx.is_perspective = state.is_perspective;
  scale_ctx.ortho_size = state.ortho_size;
  const float group_scale = computeGizmoGroupScale(scale_ctx);

  float idot[3]{};
  computeGizmoIdot(basis, state.camera_position, basis.origin, idot);

  const bool translate_session_active =
      m_controller.isTranslateModalSessionActive();
  const bool translate_grab_session =
      translate_session_active &&
      m_controller.translateModalSession().isGrabEntry();
  const bool translate_handle_feedback =
      translate_session_active && !translate_grab_session;
  const ManipulatorAxis translate_active_handle =
      translate_session_active
          ? m_controller.translateModalSession().activeHandle()
          : ManipulatorAxis::last;

  const auto draw_translate_axis = [&](const ManipulatorAxis axis) {
    // Live pivot handles stay unhighlighted during modal sessions; handle-entry
    // feedback (ghost/guides) draws at drag-start basis instead.
    const bool highlight = !translate_session_active &&
                           m_controller.isDragging() &&
                           m_controller.getActiveAxis() == axis;
    if (gizmoAxisFadeFactor(axis, idot) <= 0.0f) {
      return;
    }
    (void)drawTranslateHandle(cmd, state, axis, basis, idot, group_scale, highlight);
  };

  if (state.gizmo_mode == TransformGizmoMode::translate || translate_grab_session) {
    // Draw planes first, then arrows on top (Blender-style layering).
    const ManipulatorAxis planes[] = {ManipulatorAxis::trans_xy, ManipulatorAxis::trans_yz,
                                      ManipulatorAxis::trans_zx};
    const ManipulatorAxis arrows[] = {ManipulatorAxis::trans_x, ManipulatorAxis::trans_y,
                                      ManipulatorAxis::trans_z};
    for (const ManipulatorAxis axis : planes) {
      if (!translate_session_active ||
          translateSessionShowsPlaneHandle(translate_active_handle, axis)) {
        draw_translate_axis(axis);
      }
    }
    for (const ManipulatorAxis axis : arrows) {
      draw_translate_axis(axis);
    }
    if (!translate_session_active || translateSessionShowsCenterHandle()) {
      draw_translate_axis(ManipulatorAxis::trans_c);
    }

    if (translate_handle_feedback) {
      const TranslateModalSession& session =
          m_controller.translateModalSession();
      const GizmoBasis& start_basis = session.dragStartBasis();
      float start_idot[3]{};
      computeGizmoIdot(start_basis, state.camera_position, start_basis.origin,
                       start_idot);

      TransformGizmoScaleContext start_scale_ctx{};
      start_scale_ctx.view_projection = state.projection * state.view;
      start_scale_ctx.pivot = start_basis.origin;
      start_scale_ctx.viewport_height =
          static_cast<float>(state.viewport_height);
      start_scale_ctx.is_perspective = state.is_perspective;
      start_scale_ctx.ortho_size = state.ortho_size;
      const float start_group_scale =
          computeGizmoGroupScale(start_scale_ctx);

      const float guide_idot[3] = {1.0f, 1.0f, 1.0f};
      ManipulatorAxis guide_axes[2] = {ManipulatorAxis::last,
                                       ManipulatorAxis::last};
      const uint32_t guide_count =
          translateSessionGuideAxes(translate_active_handle, guide_axes);
      for (uint32_t i = 0; i < guide_count; ++i) {
        (void)drawTranslateHandle(
            cmd, state, guide_axes[i], start_basis, guide_idot,
            start_group_scale * k_translate_guide_scale_multiplier, false,
            k_translate_guide_alpha);
      }

      (void)drawTranslateHandle(cmd, state, translate_active_handle, start_basis,
                                start_idot, start_group_scale, true,
                                k_translate_ghost_alpha);

      const ManipulatorAxis dot_color_axis =
          translateSessionOriginColorAxis(translate_active_handle);
      if (dot_color_axis != ManipulatorAxis::last) {
        GizmoBasis dot_basis = basis;
        if (isPlaneManipulator(translate_active_handle)) {
          dot_basis.origin = translatePlaneHandleCenter(
              basis, translate_active_handle,
              computeGizmoHandleScale(group_scale, translate_active_handle));
        }
        drawTranslateOriginDot(cmd, state, dot_basis, group_scale,
                               dot_color_axis);
      }
    }
  } else if (state.gizmo_mode == TransformGizmoMode::scale) {
    const ManipulatorAxis axes[] = {ManipulatorAxis::trans_x, ManipulatorAxis::trans_y,
                                    ManipulatorAxis::trans_z, ManipulatorAxis::trans_c};
    for (const ManipulatorAxis axis : axes) {
      const bool highlight =
          m_controller.isDragging() && m_controller.getActiveAxis() == axis;
      if (gizmoAxisFadeFactor(axis, idot) <= 0.0f) {
        continue;
      }
      (void)drawScaleHandle(cmd, state, axis, basis, idot, group_scale, highlight);
    }
  } else if (state.gizmo_mode == TransformGizmoMode::rotate) {
    const ManipulatorAxis axes[] = {ManipulatorAxis::rot_x, ManipulatorAxis::rot_y,
                                    ManipulatorAxis::rot_z};
    for (const ManipulatorAxis axis : axes) {
      const bool highlight =
          m_controller.isDragging() && m_controller.getActiveAxis() == axis;
      drawRotationDial(cmd, state, axis, basis, idot, group_scale, highlight, false);
    }

    if (m_controller.isDragging() &&
        isRotationManipulator(m_controller.getActiveAxis())) {
      const ManipulatorAxis axis = m_controller.getActiveAxis();
      drawRotationDial(cmd, state, axis, basis, idot, group_scale, true, true);
    }
  }
}

bool TransformGizmoOverlay::drawTranslateHandle(VkCommandBuffer cmd,
                                                const OverlayState& state,
                                                ManipulatorAxis axis,
                                                const GizmoBasis& basis, const float idot[3],
                                                float group_scale, bool highlight,
                                                const float alpha_scale) {
  const float fade = gizmoAxisFadeFactor(axis, idot);
  if (fade <= 0.0f) {
    return false;
  }

  const GizmoDrawStyle draw_style = styleForTranslateAxis(axis);
  const GizmoAxisColor axis_color = gizmoAxisColor(axis, idot);
  const glm::vec4 color = highlight ? axis_color.color_hi : axis_color.color;
  const float alpha = std::max(color.a, 0.85f) * alpha_scale;
  const float line_width_scale = gizmoLineWidthScale(axis, draw_style);

  const float handle_scale = computeGizmoHandleScale(group_scale, axis);
  const glm::mat4 handle_matrix =
      axis == ManipulatorAxis::trans_c
          ? gizmoViewAlignedCenterMatrix(basis, handle_scale, state.camera_position)
          : gizmoHandleMatrix(basis, axis, handle_scale);

  if (axis == ManipulatorAxis::trans_c) {
    recordDraw(cmd, state, handle_matrix, glm::vec4(glm::vec3(color), 1.0f),
               GizmoDrawStyle::plane, alpha, k_center_quad_layout, k_plane_quad_z,
               line_width_scale);
    return true;
  }

  recordDraw(cmd, state, handle_matrix, glm::vec4(glm::vec3(color), 1.0f), draw_style,
             alpha, k_plane_quad_layout, k_plane_quad_z, line_width_scale);
  return true;
}

void TransformGizmoOverlay::drawTranslateOriginDot(
    VkCommandBuffer cmd, const OverlayState& state, const GizmoBasis& basis,
    const float group_scale, const ManipulatorAxis color_axis) {
  const glm::vec4 color = axisColorFor(color_axis, true);
  const float handle_scale =
      computeGizmoHandleScale(group_scale, ManipulatorAxis::trans_c) *
      k_translate_origin_dot_scale;
  const glm::mat4 handle_matrix =
      gizmoViewAlignedCenterMatrix(basis, handle_scale, state.camera_position);
  recordDraw(cmd, state, handle_matrix, glm::vec4(glm::vec3(color), 1.0f),
             GizmoDrawStyle::plane, 1.0f, k_center_quad_layout, k_plane_quad_z,
             gizmoLineWidthScale(ManipulatorAxis::trans_c, GizmoDrawStyle::plane));
}

bool TransformGizmoOverlay::drawScaleHandle(VkCommandBuffer cmd, const OverlayState& state,
                                            ManipulatorAxis axis, const GizmoBasis& basis,
                                            const float idot[3], float group_scale,
                                            bool highlight) {
  const float fade = gizmoAxisFadeFactor(axis, idot);
  if (fade <= 0.0f) {
    return false;
  }

  const GizmoAxisColor axis_color = gizmoAxisColor(axis, idot);
  const glm::vec4 color = highlight ? axis_color.color_hi : axis_color.color;
  const float alpha = std::max(color.a, 0.85f);
  const float line_width_scale = gizmoLineWidthScale(axis, GizmoDrawStyle::plane);
  const float handle_scale = computeGizmoHandleScale(group_scale, axis);
  const glm::mat4 handle_matrix =
      axis == ManipulatorAxis::trans_c
          ? gizmoViewAlignedCenterMatrix(basis, handle_scale, state.camera_position)
          : gizmoScaleBoxMatrix(basis, axis, handle_scale);
  const glm::vec4 quad_layout =
      axis == ManipulatorAxis::trans_c ? k_center_quad_layout : k_scale_tip_quad_layout;
  const float quad_z =
      axis == ManipulatorAxis::trans_c ? k_scale_center_quad_z : k_scale_tip_quad_z;
  recordDraw(cmd, state, handle_matrix, glm::vec4(glm::vec3(color), 1.0f),
             GizmoDrawStyle::plane, alpha, quad_layout, quad_z, line_width_scale);
  return true;
}

void TransformGizmoOverlay::drawRotationDial(VkCommandBuffer cmd,
                                              const OverlayState& state,
                                              ManipulatorAxis axis,
                                              const GizmoBasis& basis, const float idot[3],
                                              float group_scale, bool highlight, bool ghost) {
  const GizmoDrawStyle style = ghost ? GizmoDrawStyle::dial_ghost : GizmoDrawStyle::dial;
  const GizmoAxisColor axis_color = gizmoAxisColor(axis, idot);
  glm::vec4 color = highlight ? axis_color.color_hi : axis_color.color;
  float alpha = color.a;
  if (ghost) {
    color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    alpha = 0.5f;
  }

  const float line_width_scale = gizmoLineWidthScale(axis, style);
  const float handle_scale = computeGizmoHandleScale(group_scale, axis);
  const glm::mat4 handle_matrix = gizmoDialMatrix(basis, axis, handle_scale);
  float arc_start = 0.0f;
  float arc_delta = 0.0f;
  if (ghost) {
    arc_start = m_controller.getRotationArcMeshBase();
    arc_delta =
        m_controller.getRotationDragCurrentAngle() - m_controller.getRotationDragStartAngle();
  }

  recordDraw(cmd, state, handle_matrix, glm::vec4(glm::vec3(color), 1.0f), style, alpha,
             k_plane_quad_layout, k_plane_quad_z, line_width_scale, arc_start, arc_delta);
}

void TransformGizmoOverlay::recordDraw(VkCommandBuffer cmd, const OverlayState& state,
                                     const glm::mat4& gizmo_world, const glm::vec4& color,
                                     GizmoDrawStyle style, float alpha,
                                     const glm::vec4& quad_layout, float quad_z,
                                     float line_width_scale, float arc_start,
                                     float arc_delta) {
  TransformGizmoUniformData uniform =
      makeUniform(state, gizmo_world, color, style, alpha, quad_layout, quad_z,
                  line_width_scale, arc_start, arc_delta);

  ASSERT(m_next_draw_slot < k_max_gizmo_draws_per_frame);
  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t slot_index =
      (state.frame_index % frames) * k_max_gizmo_draws_per_frame + m_next_draw_slot;
  ++m_next_draw_slot;
  ASSERT(slot_index < m_uniform_buffers.size());

  m_uniform_buffers[slot_index]->upload(&uniform, sizeof(uniform));

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->nativePipeline()->getGraphicsPipeline());
  const VkDescriptorSet descriptor_set =
      reinterpret_cast<VkDescriptorSet>(m_descriptor_sets[slot_index]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->nativePipeline()->getPipelineLayout(), 0,
                          1, &descriptor_set, 0, nullptr);

  vkCmdDraw(cmd, vertexCountForStyle(style), 1, 0, 0);
}

}  // namespace Blunder
