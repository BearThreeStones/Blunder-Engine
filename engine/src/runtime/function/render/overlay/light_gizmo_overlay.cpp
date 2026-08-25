#include "runtime/function/render/overlay/light_gizmo_overlay.h"

#include <algorithm>
#include <cmath>
#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/overlay/light_gizmo_geometry.h"
#include "runtime/function/render/overlay/light_gizmo_hit_test.h"
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
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

namespace Blunder {

namespace {

constexpr uint32_t k_max_draws_per_frame = 256u;
constexpr uint32_t k_line_vert_count = 6u;
constexpr uint32_t k_triangle_vert_count = 3u;
constexpr uint32_t k_origin_disc_vert_count = 6u;
constexpr uint32_t k_icon_vert_count = 6u;

constexpr float k_line_width_px = 0.75f;

const glm::vec4 k_muted_color{0.0f, 0.0f, 0.0f, 0.9f};
const glm::vec4 k_selected_color{1.0f, 0.6f, 0.1f, 0.9f};

struct LightGizmoUniformData {
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

static_assert(sizeof(LightGizmoUniformData) == 208u,
              "LightGizmoUniformData must match camera_gizmo.slang std140 layout");

glm::vec3 transformPoint(const glm::mat4& world, const Vec3& local) {
  const glm::vec4 h = world * glm::vec4(local.x, local.y, local.z, 1.0f);
  return glm::vec3(h);
}

LightGizmoKind kindFromLight(LightType type) {
  switch (type) {
    case LightType::point:
      return LightGizmoKind::point;
    case LightType::spot:
      return LightGizmoKind::spot;
    case LightType::area:
      return LightGizmoKind::area;
    case LightType::directional:
    default:
      return LightGizmoKind::directional;
  }
}

LightGizmoShape shapeFromLight(const LightComponent& light) {
  LightGizmoShape shape{};
  shape.kind = kindFromLight(light.type);
  shape.range = light.range;
  shape.outer_cone_degrees = light.outer_cone_degrees;
  shape.width = light.width;
  shape.height = light.height;
  return shape;
}

enum class LightGizmoDrawStyle : uint32_t {
  line = 0,
  triangle = 1,
  origin_disc = 2,
  icon_billboard = 3,
};

uint32_t vertexCountForStyle(LightGizmoDrawStyle style) {
  switch (style) {
    case LightGizmoDrawStyle::line:
      return k_line_vert_count;
    case LightGizmoDrawStyle::triangle:
      return k_triangle_vert_count;
    case LightGizmoDrawStyle::origin_disc:
      return k_origin_disc_vert_count;
    case LightGizmoDrawStyle::icon_billboard:
      return k_icon_vert_count;
    default:
      return 0u;
  }
}

}  // namespace

LightGizmoOverlay::~LightGizmoOverlay() {
  shutdown();
}

void LightGizmoOverlay::initialize(const OverlayResources& res,
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
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(LightGizmoUniformData),
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
    LOG_FATAL("[LightGizmo] vkCreateDescriptorPool failed: {}",
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
    LOG_FATAL("[LightGizmo] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(total_sets);
  for (uint32_t i = 0; i < total_sets; ++i) {
    m_descriptor_sets[i] = reinterpret_cast<uintptr_t>(sets[i]);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(LightGizmoUniformData);

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

void LightGizmoOverlay::shutdown() {
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

void LightGizmoOverlay::begin_sync(OverlayResources& /*res*/,
                                   const OverlayState& /*state*/) {
  enabled_ = true;
}

void LightGizmoOverlay::recordDraw(VkCommandBuffer cmd, const OverlayState& state,
                                   DrawStyle style, const glm::vec3& p0,
                                   const glm::vec3& p1, const glm::vec3& p2,
                                   const glm::vec4& color) {
  if (m_next_draw_slot >= k_max_draws_per_frame) {
    return;
  }

  LightGizmoUniformData uniform{};
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
  vkCmdDraw(cmd, vertexCountForStyle(static_cast<LightGizmoDrawStyle>(style)), 1,
            0, 0);
}

void LightGizmoOverlay::draw_screen(VkCommandBuffer cmd,
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

  EditorSelectionSystem* selection =
      g_runtime_global_context.m_editor_selection.get();

  scene->forEachLight([&](EntityId entity_id, const LightComponent& light) {
    if (m_next_draw_slot >= k_max_draws_per_frame) {
      return;
    }

    const bool selected = selection != nullptr && selection->isSelected(entity_id);
    const glm::vec4 color = selected ? k_selected_color : k_muted_color;
    const glm::mat4 world = scene->getWorldMatrix(entity_id);
    const LightGizmoShape shape = shapeFromLight(light);

    forEachLightGizmoSegmentLocal(shape, [&](const Vec3& a, const Vec3& b) {
      recordDraw(cmd, state, DrawStyle::line, transformPoint(world, a),
                 transformPoint(world, b), glm::vec3(0.0f), color);
    });
  });
}

std::optional<OverlayGizmoPickHit> LightGizmoOverlay::hitTest(
    const Vec2& window_position, EditorCamera& camera) const {
  if (!enabled_ || !camera.isWindowPositionInViewport(window_position)) {
    return std::nullopt;
  }

  if (!g_runtime_global_context.m_scene_system) {
    return std::nullopt;
  }
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    return std::nullopt;
  }

  const Vec2 viewport_local = camera.windowToViewportLocal(window_position);
  const glm::vec2 pointer(viewport_local.x, viewport_local.y);
  const float vp_w = camera.getViewportWidth();
  const float vp_h = std::max(camera.getViewportHeight(), 1.0f);
  const glm::mat4 view = camera.getViewMatrix();
  const glm::mat4 proj = camera.getProjectionMatrix();

  EntityId best_entity{k_invalid_entity_id};
  float best_depth = -1e9f;

  scene->forEachLight([&](EntityId entity_id, const LightComponent& light) {
    const LightGizmoShape shape = shapeFromLight(light);
    const glm::mat4 world = scene->getWorldMatrix(entity_id);
    const std::optional<float> hit_depth = hitTestLightGizmoViewportLocal(
        pointer, shape, world, view, proj, vp_w, vp_h);
    if (!hit_depth.has_value() ||
        !overlayGizmoViewDepthIsCloser(hit_depth.value(), best_depth)) {
      return;
    }
    best_depth = hit_depth.value();
    best_entity = entity_id;
  });

  if (!isValid(best_entity)) {
    return std::nullopt;
  }
  return OverlayGizmoPickHit{best_entity, best_depth};
}

}  // namespace Blunder
