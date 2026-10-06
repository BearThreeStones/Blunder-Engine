#include "runtime/function/render/overlay/light_gizmo_overlay.h"

#include <algorithm>
#include <cmath>
#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/geometry.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/gpu_driven/gpu_driven_cull.h"
#include "runtime/function/render/overlay/collision_gizmo_geometry.h"
#include "runtime/function/render/overlay/gizmo_line_command.h"
#include "runtime/function/render/overlay/light_gizmo_geometry.h"
#include "runtime/function/render/overlay/light_gizmo_hit_test.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/render_system.h"
#include "runtime/project/play_pose_preview.h"
#include "runtime/project/play_session_controller.h"
#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

namespace Blunder {

namespace {

constexpr float k_line_width_px = 0.75f;

const glm::vec4 k_muted_color{0.0f, 0.0f, 0.0f, 0.9f};
const glm::vec4 k_selected_color{1.0f, 0.6f, 0.1f, 0.9f};
const glm::vec4 k_collision_color{0.15f, 0.85f, 1.0f, 1.0f};
const glm::vec4 k_collision_selected{1.0f, 0.6f, 0.1f, 1.0f};

const PlayPoseOverlayMap* activePlayPoseOverlay() {
  PlaySessionController* session = g_runtime_global_context.m_play_session.get();
  if (session == nullptr) {
    return nullptr;
  }
  if (session->state() != PlaySessionState::Playing &&
      session->state() != PlaySessionState::Paused) {
    return nullptr;
  }
  if (session->poseOverlay().empty()) {
    return nullptr;
  }
  return &session->poseOverlay();
}

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

Mat4 gizmoWorldForEntity(SceneInstance& scene, EntityId entity_id,
                         LightGizmoKind kind) {
  const Mat4 unique_world = scene.getWorldMatrix(entity_id);
  Mat4 parent_world(1.0f);
  Vec3 unique_local(unique_world[3]);
  if (const Entity* entity = scene.getEntity(entity_id); entity != nullptr) {
    unique_local = entity->getPosition();
    if (isValid(entity->getParentId())) {
      parent_world = scene.getWorldMatrix(entity->getParentId());
    }
  }
  return makeLightGizmoWorldMatchingMesh(
      unique_world, parent_world, kind, sceneDrawnMeshIsCentimetre(scene),
      unique_local);
}

}  // namespace

LightGizmoOverlay::~LightGizmoOverlay() {
  shutdown();
}

void LightGizmoOverlay::initialize(const OverlayResources& res,
                                   SlangCompiler* compiler) {
  m_batch.initialize(res, compiler);
}

void LightGizmoOverlay::shutdown() {
  m_batch.shutdown();
}

void LightGizmoOverlay::begin_sync(OverlayResources& /*res*/,
                                   const OverlayState& /*state*/) {
  enabled_ = true;
}

void LightGizmoOverlay::draw_screen(VkCommandBuffer cmd,
                                    const OverlayState& state) {
  if (!enabled_) {
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

  m_batch.begin();

  EditorSelectionSystem* selection =
      g_runtime_global_context.m_editor_selection.get();

  scene->tick(0.0f);

  glm::vec4 frustum[6];
  extractFrustumPlanes(state.projection * state.view, frustum);

  scene->forEachLight([&](EntityId entity_id, const LightComponent& light) {
    if (!scene->isActiveInHierarchy(entity_id)) {
      return;
    }

    const bool selected = selection != nullptr && selection->isSelected(entity_id);
    const glm::vec4 color = selected ? k_selected_color : k_muted_color;
    LightGizmoShape shape = shapeFromLight(light);
    shape.range = gltfMetreQuantityInWorld(light.range, sceneDrawnMeshIsCentimetre(*scene));
    shape.show_range = selected;
    const glm::mat4 world = gizmoWorldForEntity(*scene, entity_id, shape.kind);
    const glm::vec3 origin = overlayGizmoWorldOrigin(world);
    const float radius =
        lightGizmoLocalCullRadius(shape) * gizmoWorldScaleMax(world);
    if (!gizmoSphereVisible(origin, radius, frustum)) {
      return;
    }
    const glm::vec4 icon_color =
        selected ? glm::vec4(k_selected_color.x, k_selected_color.y,
                             k_selected_color.z, 1.0f)
                 : glm::vec4(0.0f, 0.0f, 0.0f, 0.95f);
    m_batch.push(GizmoLineDrawStyle::light_icon_billboard, origin, glm::vec3(0.0f),
                 glm::vec3(0.0f), icon_color);

    forEachLightGizmoSegmentLocal(shape, [&](const Vec3& a, const Vec3& b) {
      m_batch.push(GizmoLineDrawStyle::line, transformPoint(world, a),
                   transformPoint(world, b), glm::vec3(0.0f), color);
    });
  });

  const PlayPoseOverlayMap* pose_overlay = activePlayPoseOverlay();
  const bool draw_collision =
      g_runtime_global_context.m_render_system != nullptr &&
      g_runtime_global_context.m_render_system->areCollisionGizmosVisible();
  if (draw_collision) {
    scene->forEachCollider([&](EntityId entity_id, const ColliderComponent& collider) {
      if (!scene->isActiveInHierarchy(entity_id)) {
        return;
      }

      const bool selected = selection != nullptr && selection->isSelected(entity_id);
      const glm::vec4 color = selected ? k_collision_selected : k_collision_color;
      const glm::mat4 world =
          worldMatrixWithPlayPoseOverlay(*scene, entity_id, pose_overlay);
      float local_radius = 0.0f;
      if (colliderGizmoTryLocalCullRadius(collider, local_radius)) {
        const glm::vec3 origin = overlayGizmoWorldOrigin(world);
        const float radius = local_radius * gizmoWorldScaleMax(world);
        if (!gizmoSphereVisible(origin, radius, frustum)) {
          return;
        }
      }
      forEachColliderWireSegment(collider, [&](const Vec3& a, const Vec3& b) {
        m_batch.push(GizmoLineDrawStyle::line, transformPoint(world, a),
                     transformPoint(world, b), glm::vec3(0.0f), color);
      });
    });

    scene->forEachCharacterController(
        [&](EntityId entity_id, const CharacterControllerComponent& cct) {
          if (!scene->isActiveInHierarchy(entity_id)) {
            return;
          }

          const bool selected =
              selection != nullptr && selection->isSelected(entity_id);
          const glm::vec4 color = selected ? k_collision_selected : k_collision_color;
          const glm::mat4 world =
              worldMatrixWithPlayPoseOverlay(*scene, entity_id, pose_overlay);
          const Vec3 offset = cct.shape_offset;
          const glm::vec3 origin = transformPoint(world, offset);
          const float radius =
              characterControllerLocalCullRadius(cct) * gizmoWorldScaleMax(world);
          if (!gizmoSphereVisible(origin, radius, frustum)) {
            return;
          }
          forEachCharacterControllerWireSegment(cct, [&](const Vec3& a, const Vec3& b) {
            m_batch.push(GizmoLineDrawStyle::line, transformPoint(world, a + offset),
                         transformPoint(world, b + offset), glm::vec3(0.0f), color);
          });
        });
  }

  m_batch.flush(cmd, state, k_line_width_px);
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
  EditorSelectionSystem* selection =
      g_runtime_global_context.m_editor_selection.get();
  scene->tick(0.0f);

  scene->forEachLight([&](EntityId entity_id, const LightComponent& light) {
    if (!scene->isActiveInHierarchy(entity_id)) {
      return;
    }
    LightGizmoShape shape = shapeFromLight(light);
    const bool selected = selection != nullptr && selection->isSelected(entity_id);
    shape.range = gltfMetreQuantityInWorld(light.range, sceneDrawnMeshIsCentimetre(*scene));
    shape.show_range = selected;
    const glm::mat4 world = gizmoWorldForEntity(*scene, entity_id, shape.kind);
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
