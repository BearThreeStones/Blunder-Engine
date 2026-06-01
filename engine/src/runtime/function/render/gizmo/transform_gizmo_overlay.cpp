#include "runtime/function/render/gizmo/transform_gizmo_overlay.h"

#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/gizmo/transform_gizmo_shared.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/render/offscreen_render_target.h"
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

TransformGizmoUniformData makeUniform(const OverlayState& state,
                                      const glm::mat4& gizmo_world,
                                      const glm::vec4& color, GizmoDrawStyle style,
                                      float alpha, float arc_start = 0.0f,
                                      float arc_delta = 0.0f) {
  TransformGizmoUniformData u{};
  u.view = state.view;
  u.proj = state.projection;
  u.gizmo_world = gizmo_world;
  u.color = color;
  u.params = glm::vec4(static_cast<float>(static_cast<uint32_t>(style)), alpha,
                       arc_start, arc_delta);
  return u;
}

GizmoDrawStyle styleForTranslateAxis(ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::trans_xy:
    case ManipulatorAxis::trans_yz:
    case ManipulatorAxis::trans_zx:
      return GizmoDrawStyle::plane;
    case ManipulatorAxis::trans_c:
      return GizmoDrawStyle::center;
    default:
      return GizmoDrawStyle::arrow;
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
    default:
      return TransformGizmoDrawCounts::k_arrow_verts;
  }
}

}  // namespace

TransformGizmoOverlay::~TransformGizmoOverlay() {
  shutdown();
}

void TransformGizmoOverlay::initialize(const OverlayResources& res,
                                       SlangCompiler* compiler,
                                       OffscreenRenderTarget* offscreen_target) {
  ASSERT(res.vk_context);
  ASSERT(res.vk_allocator);
  ASSERT(compiler);
  ASSERT(offscreen_target);

  m_vk_context = res.vk_context;
  m_vk_allocator = res.vk_allocator;

  rhi::GraphicsPipelineDesc desc{};
  desc.shader_path = "engine/shaders/transform_gizmo.slang";
  desc.enable_vertex_input = false;
  desc.topology = rhi::PrimitiveTopology::TriangleList;
  desc.cull_mode = rhi::CullMode::None;
  desc.enable_blend = true;
  desc.enable_depth_test = false;
  desc.enable_depth_write = false;

  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(m_vk_context, compiler);
  m_pipeline->initializeWithRenderPass(offscreen_target->getRenderPass(), desc);

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  m_uniform_buffers.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(TransformGizmoUniformData),
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
    LOG_FATAL("[TransformGizmo] vkCreateDescriptorPool failed: {}",
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
    LOG_FATAL("[TransformGizmo] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
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
  enabled_ = state.has_selection &&
             (state.gizmo_mode == TransformGizmoMode::translate ||
              state.gizmo_mode == TransformGizmoMode::rotate);
}

void TransformGizmoOverlay::draw_color_only(VkCommandBuffer cmd,
                                            const OverlayState& state) {
  if (!enabled_ || m_pipeline == nullptr || !state.has_selection) {
    return;
  }

  GizmoBasis basis{};
  if (!m_controller.buildActiveGizmoBasis(basis)) {
    return;
  }

  const float scale = computeHandleUniformScale(
      state.camera_position, basis.origin, state.viewport_height,
      state.vertical_fov, state.is_perspective, state.ortho_size);

  if (state.gizmo_mode == TransformGizmoMode::translate) {
    const ManipulatorAxis axes[] = {
        ManipulatorAxis::trans_x, ManipulatorAxis::trans_y, ManipulatorAxis::trans_z,
        ManipulatorAxis::trans_xy, ManipulatorAxis::trans_yz, ManipulatorAxis::trans_zx,
        ManipulatorAxis::trans_c,
    };
    for (const ManipulatorAxis axis : axes) {
      const bool highlight =
          m_controller.isDragging() && m_controller.getActiveAxis() == axis;
      drawTranslateHandle(cmd, state, axis, basis, scale, highlight);
    }
    return;
  }

  if (state.gizmo_mode == TransformGizmoMode::rotate) {
    const ManipulatorAxis axes[] = {ManipulatorAxis::rot_x, ManipulatorAxis::rot_y,
                                    ManipulatorAxis::rot_z};
    for (const ManipulatorAxis axis : axes) {
      const bool highlight =
          m_controller.isDragging() && m_controller.getActiveAxis() == axis;
      drawRotationDial(cmd, state, axis, basis, scale, highlight, false);
    }

    if (m_controller.isDragging() &&
        isRotationManipulator(m_controller.getActiveAxis())) {
      const ManipulatorAxis axis = m_controller.getActiveAxis();
      drawRotationDial(cmd, state, axis, basis, scale, true, true);
    }
  }
}

void TransformGizmoOverlay::drawTranslateHandle(VkCommandBuffer cmd,
                                                const OverlayState& state,
                                                ManipulatorAxis axis,
                                                const GizmoBasis& basis, float scale,
                                                bool highlight) {
  const GizmoDrawStyle draw_style = styleForTranslateAxis(axis);
  glm::vec3 axis_dir = basis.axis_z;
  if (axis == ManipulatorAxis::trans_x) {
    axis_dir = basis.axis_x;
  } else if (axis == ManipulatorAxis::trans_y) {
    axis_dir = basis.axis_y;
  } else if (axis == ManipulatorAxis::trans_z) {
    axis_dir = basis.axis_z;
  }

  float alpha = 1.0f;
  if (draw_style == GizmoDrawStyle::arrow) {
    alpha = viewAlignedAxisAlpha(axis_dir, state.camera_forward,
                                 state.camera_position, basis.origin);
  } else if (draw_style == GizmoDrawStyle::plane) {
    alpha = 0.55f;
  }

  glm::vec4 color = axisColorFor(axis, highlight);
  color.a *= alpha;

  const glm::mat4 handle_matrix = gizmoHandleMatrix(basis, axis, scale);
  recordDraw(cmd, state, handle_matrix, color, draw_style, alpha);
}

void TransformGizmoOverlay::drawRotationDial(VkCommandBuffer cmd,
                                             const OverlayState& state,
                                             ManipulatorAxis axis,
                                             const GizmoBasis& basis, float scale,
                                             bool highlight, bool ghost) {
  const glm::vec3 axis_dir = rotationAxisFor(axis, basis);
  float alpha =
      viewAlignedAxisAlpha(axis_dir, state.camera_forward, state.camera_position,
                           basis.origin);
  if (ghost) {
    alpha = 0.95f;
  }

  glm::vec4 color = axisColorFor(axis, highlight);
  color.a *= alpha;

  const glm::mat4 handle_matrix = gizmoDialMatrix(basis, axis, scale);
  float arc_start = 0.0f;
  float arc_delta = 0.0f;
  if (ghost) {
    arc_start = m_controller.getRotationArcMeshBase();
    arc_delta =
        m_controller.getRotationDragCurrentAngle() - m_controller.getRotationDragStartAngle();
  }

  const GizmoDrawStyle style = ghost ? GizmoDrawStyle::dial_ghost : GizmoDrawStyle::dial;
  recordDraw(cmd, state, handle_matrix, color, style, alpha, arc_start, arc_delta);
}

void TransformGizmoOverlay::recordDraw(VkCommandBuffer cmd, const OverlayState& state,
                                     const glm::mat4& gizmo_world, const glm::vec4& color,
                                     GizmoDrawStyle style, float alpha,
                                     float arc_start, float arc_delta) {
  TransformGizmoUniformData uniform =
      makeUniform(state, gizmo_world, color, style, alpha, arc_start, arc_delta);

  const uint32_t frame =
      state.frame_index % static_cast<uint32_t>(m_uniform_buffers.size());
  m_uniform_buffers[frame]->upload(&uniform, sizeof(uniform));

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->nativePipeline()->getGraphicsPipeline());
  const VkDescriptorSet descriptor_set =
      reinterpret_cast<VkDescriptorSet>(m_descriptor_sets[frame]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->nativePipeline()->getPipelineLayout(), 0,
                          1, &descriptor_set, 0, nullptr);

  vkCmdDraw(cmd, vertexCountForStyle(style), 1, 0, 0);
}

}  // namespace Blunder
