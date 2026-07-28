#include "runtime/function/render/overlay/camera_gizmo_overlay.h"

#include <algorithm>
#include <cmath>
#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/viewport_pick_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/overlay/camera_gizmo_geometry.h"
#include "runtime/function/render/overlay/camera_gizmo_hit_test.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

namespace Blunder {

namespace {

constexpr uint32_t k_max_draws_per_frame = 128u;
constexpr uint32_t k_line_vert_count = 6u;
constexpr uint32_t k_triangle_vert_count = 3u;
constexpr uint32_t k_origin_disc_vert_count = 6u;

constexpr float k_line_width_px = 1.5f;
constexpr float k_origin_cross_half_len = 0.05f;

const glm::vec4 k_muted_color{0.55f, 0.55f, 0.55f, 0.45f};
const glm::vec4 k_selected_color{1.0f, 0.6f, 0.1f, 0.9f};
const glm::vec4 k_handle_color{1.0f, 0.85f, 0.2f, 1.0f};

/// Must match CameraGizmoUniform in camera_gizmo.slang (std140).
struct CameraGizmoUniformData {
  glm::mat4 view{1.0f};
  glm::mat4 proj{1.0f};
  glm::vec4 color{1.0f};
  float line_width_px{0.0f};
  float viewport_height_px{1.0f};
  float style{0.0f};
  float _pad{0.0f};
  glm::vec4 p0{0.0f};
  glm::vec4 p1{0.0f};
  glm::vec4 p2{0.0f};
};

static_assert(sizeof(CameraGizmoUniformData) == 208u,
              "CameraGizmoUniformData must match camera_gizmo.slang std140 layout");

glm::vec3 transformPoint(const glm::mat4& world, const Vec3& local) {
  const glm::vec4 h = world * glm::vec4(local.x, local.y, local.z, 1.0f);
  return glm::vec3(h);
}

enum class CameraGizmoDrawStyle : uint32_t {
  line = 0,
  triangle = 1,
  origin_disc = 2,
};

uint32_t vertexCountForStyle(CameraGizmoDrawStyle style) {
  switch (style) {
    case CameraGizmoDrawStyle::line:
      return k_line_vert_count;
    case CameraGizmoDrawStyle::triangle:
      return k_triangle_vert_count;
    case CameraGizmoDrawStyle::origin_disc:
      return k_origin_disc_vert_count;
    default:
      return 0u;
  }
}

}  // namespace

CameraGizmoOverlay::~CameraGizmoOverlay() {
  shutdown();
}

void CameraGizmoOverlay::initialize(const OverlayResources& res,
                                    SlangCompiler* compiler) {
  ASSERT(res.vk_context);
  ASSERT(res.vk_allocator);
  ASSERT(compiler);
  ASSERT(res.screen_render_pass != VK_NULL_HANDLE);

  m_vk_context = res.vk_context;
  m_vk_allocator = res.vk_allocator;

  rhi::GraphicsPipelineDesc desc{};
  desc.shader_path = "engine/shaders/camera_gizmo.slang";
  desc.enable_vertex_input = false;
  desc.topology = rhi::PrimitiveTopology::TriangleList;
  desc.cull_mode = rhi::CullMode::None;
  desc.enable_blend = true;
  desc.enable_depth_test = false;
  desc.enable_depth_write = false;

  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(m_vk_context, compiler);
  m_pipeline->initializeWithRenderPass(res.screen_render_pass, desc);

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t total_sets = frames * k_max_draws_per_frame;
  m_uniform_buffers.resize(total_sets);
  for (uint32_t i = 0; i < total_sets; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(CameraGizmoUniformData),
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
    LOG_FATAL("[CameraGizmo] vkCreateDescriptorPool failed: {}",
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
    LOG_FATAL("[CameraGizmo] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(total_sets);
  for (uint32_t i = 0; i < total_sets; ++i) {
    m_descriptor_sets[i] = reinterpret_cast<uintptr_t>(sets[i]);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(CameraGizmoUniformData);

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

void CameraGizmoOverlay::shutdown() {
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

void CameraGizmoOverlay::begin_sync(OverlayResources& /*res*/,
                                    const OverlayState& /*state*/) {
  enabled_ = true;
}

void CameraGizmoOverlay::recordDraw(VkCommandBuffer cmd, const OverlayState& state,
                                    DrawStyle style, const glm::vec3& p0,
                                    const glm::vec3& p1, const glm::vec3& p2,
                                    const glm::vec4& color) {
  if (m_next_draw_slot >= k_max_draws_per_frame) {
    return;
  }

  CameraGizmoUniformData uniform{};
  uniform.view = state.view;
  uniform.proj = state.projection;
  uniform.color = color;
  uniform.line_width_px = k_line_width_px;
  uniform.viewport_height_px =
      std::max(static_cast<float>(state.viewport_height), 1.0f);
  uniform.style = static_cast<float>(static_cast<uint32_t>(style));
  uniform.p0 = glm::vec4(p0, 1.0f);
  uniform.p1 = glm::vec4(p1, 1.0f);
  uniform.p2 = glm::vec4(p2, 1.0f);

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t slot_index =
      (state.frame_index % frames) * k_max_draws_per_frame + m_next_draw_slot;
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
  vkCmdDraw(cmd, vertexCountForStyle(static_cast<CameraGizmoDrawStyle>(style)), 1,
            0, 0);
}

void CameraGizmoOverlay::draw_screen(VkCommandBuffer cmd,
                                     const OverlayState& state) {
  if (!enabled_ || m_pipeline == nullptr) {
    return;
  }

  if (!g_runtime_global_context.m_scene_system) {
    return;
  }
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
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

  m_next_draw_slot = 0;

  const float aspect =
      static_cast<float>(state.viewport_width) /
      std::max(static_cast<float>(state.viewport_height), 1.0f);

  EditorSelectionSystem* selection =
      g_runtime_global_context.m_editor_selection.get();

  EntityId sole_selected_camera{k_invalid_entity_id};
  if (selection != nullptr) {
    const eastl::vector<EntityId> selected_ids = selection->getSelectedIds();
    if (selected_ids.size() == 1 && scene->getCamera(selected_ids[0]) != nullptr) {
      sole_selected_camera = selected_ids[0];
    }
  }

  scene->forEachCamera([&](EntityId entity_id, const CameraComponent& camera) {
    if (m_next_draw_slot >= k_max_draws_per_frame) {
      return;
    }

    const bool selected = selection != nullptr && selection->isSelected(entity_id);
    const glm::vec4 color = selected ? k_selected_color : k_muted_color;

    const float fov_rad = glm::radians(camera.vertical_fov_degrees);
    const CameraGizmoFrame frame = buildCameraGizmoFrameLocal(
        fov_rad, aspect, kCameraGizmoDisplayDistance);

    const glm::mat4 world = scene->getWorldMatrix(entity_id);
    const glm::vec3 origin = transformPoint(world, frame.origin);

    glm::vec3 corners[4];
    for (int i = 0; i < 4; ++i) {
      corners[i] = transformPoint(world, frame.corners[i]);
    }

    for (int i = 0; i < 4; ++i) {
      recordDraw(cmd, state, DrawStyle::line, origin, corners[i], glm::vec3(0.0f),
                 color);
    }

    for (int i = 0; i < 4; ++i) {
      const int next = (i + 1) % 4;
      recordDraw(cmd, state, DrawStyle::line, corners[i], corners[next],
                 glm::vec3(0.0f), color);
    }

    glm::vec3 tri[3];
    for (int i = 0; i < 3; ++i) {
      tri[i] = transformPoint(world, frame.up_triangle[i]);
    }
    recordDraw(cmd, state, DrawStyle::triangle, tri[0], tri[1], tri[2], color);

    recordDraw(cmd, state, DrawStyle::origin_disc, origin, glm::vec3(0.0f),
               glm::vec3(0.0f), color);

    const Vec3 local_x{k_origin_cross_half_len, 0.0f, 0.0f};
    const Vec3 local_y{0.0f, k_origin_cross_half_len, 0.0f};
    recordDraw(cmd, state, DrawStyle::line,
               transformPoint(world, Vec3(-local_x.x, -local_x.y, -local_x.z)),
               transformPoint(world, local_x), glm::vec3(0.0f), color);
    recordDraw(cmd, state, DrawStyle::line,
               transformPoint(world, Vec3(-local_y.x, -local_y.y, -local_y.z)),
               transformPoint(world, local_y), glm::vec3(0.0f), color);

    if (entity_id != sole_selected_camera) {
      return;
    }

    const float fov_rad_handles = glm::radians(camera.vertical_fov_degrees);
    const CameraGizmoFrame near_frame =
        buildCameraGizmoFrameLocal(fov_rad_handles, aspect, camera.near_clip);
    const CameraGizmoFrame far_frame =
        buildCameraGizmoFrameLocal(fov_rad_handles, aspect, camera.far_clip);

    glm::vec3 near_corners[4];
    glm::vec3 far_corners[4];
    for (int i = 0; i < 4; ++i) {
      near_corners[i] = transformPoint(world, near_frame.corners[i]);
      far_corners[i] = transformPoint(world, far_frame.corners[i]);
    }
    const glm::vec3 near_origin = transformPoint(world, near_frame.origin);
    const glm::vec3 far_origin = transformPoint(world, far_frame.origin);

    for (int i = 0; i < 4; ++i) {
      const int next = (i + 1) % 4;
      recordDraw(cmd, state, DrawStyle::line, near_corners[i], near_corners[next],
                 glm::vec3(0.0f), k_handle_color);
      recordDraw(cmd, state, DrawStyle::line, far_corners[i], far_corners[next],
                 glm::vec3(0.0f), k_handle_color);
    }
    recordDraw(cmd, state, DrawStyle::line, origin, near_origin, glm::vec3(0.0f),
               k_handle_color);
    recordDraw(cmd, state, DrawStyle::line, origin, far_origin, glm::vec3(0.0f),
               k_handle_color);

    recordDraw(cmd, state, DrawStyle::line, corners[0], corners[1], glm::vec3(0.0f),
               k_handle_color);
    recordDraw(cmd, state, DrawStyle::line, tri[0], tri[1], glm::vec3(0.0f),
               k_handle_color);
    recordDraw(cmd, state, DrawStyle::line, tri[1], tri[2], glm::vec3(0.0f),
               k_handle_color);
  });
}

bool CameraGizmoOverlay::tryHandleMouseClick(const Vec2& window_position,
                                             EditorCamera& camera) {
  if (!enabled_ || !camera.isWindowPositionInViewport(window_position)) {
    return false;
  }

  if (!g_runtime_global_context.m_scene_system) {
    return false;
  }
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    return false;
  }

  const Vec2 viewport_local = camera.windowToViewportLocal(window_position);
  const glm::vec2 pointer(viewport_local.x, viewport_local.y);
  const float vp_w = camera.getViewportWidth();
  const float vp_h = std::max(camera.getViewportHeight(), 1.0f);
  const float aspect = vp_w / vp_h;
  const glm::mat4 view = camera.getViewMatrix();
  const glm::mat4 proj = camera.getProjectionMatrix();

  EntityId best_entity{k_invalid_entity_id};
  float best_depth = -1e9f;

  scene->forEachCamera([&](EntityId entity_id, const CameraComponent& cam) {
    const float fov_rad = glm::radians(cam.vertical_fov_degrees);
    const CameraGizmoFrame frame =
        buildCameraGizmoFrameLocal(fov_rad, aspect, kCameraGizmoDisplayDistance);
    const glm::mat4 world = scene->getWorldMatrix(entity_id);
    const std::optional<float> hit_depth = hitTestCameraGizmoFrameViewportLocal(
        pointer, frame, world, view, proj, vp_w, vp_h);
    if (!hit_depth.has_value() || hit_depth.value() <= best_depth) {
      return;
    }
    best_depth = hit_depth.value();
    best_entity = entity_id;
  });

  if (!isValid(best_entity)) {
    return false;
  }

  if (g_runtime_global_context.m_editor_selection) {
    g_runtime_global_context.m_editor_selection->setSelection(best_entity);
  }
  if (g_runtime_global_context.m_viewport_pick) {
    g_runtime_global_context.m_viewport_pick->suppressNextLeftReleasePick();
  }
  return true;
}

}  // namespace Blunder
