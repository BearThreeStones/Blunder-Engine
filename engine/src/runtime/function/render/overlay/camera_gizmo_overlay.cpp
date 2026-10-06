#include "runtime/function/render/overlay/camera_gizmo_overlay.h"

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
#include "runtime/function/editor/viewport_pick_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/gpu_driven/gpu_driven_cull.h"
#include "runtime/function/render/overlay/camera_gizmo_geometry.h"
#include "runtime/function/render/overlay/camera_gizmo_hit_test.h"
#include "runtime/function/render/overlay/gizmo_line_command.h"
#include "runtime/function/render/overlay/light_gizmo_geometry.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

namespace Blunder {

namespace {

constexpr float k_line_width_px = 0.75f;

const glm::vec4 k_muted_color{0.0f, 0.0f, 0.0f, 0.9f};
const glm::vec4 k_selected_color{1.0f, 0.6f, 0.1f, 0.9f};
const glm::vec4 k_handle_color{1.0f, 0.85f, 0.2f, 1.0f};

glm::vec3 transformPoint(const glm::mat4& world, const Vec3& local) {
  const glm::vec4 h = world * glm::vec4(local.x, local.y, local.z, 1.0f);
  return glm::vec3(h);
}

Mat4 gizmoWorldForEntity(SceneInstance& scene, EntityId entity_id) {
  const Mat4 unique_world = scene.getWorldMatrix(entity_id);
  Mat4 parent_world(1.0f);
  Vec3 unique_local(unique_world[3]);
  if (const Entity* entity = scene.getEntity(entity_id); entity != nullptr) {
    unique_local = entity->getPosition();
    if (isValid(entity->getParentId())) {
      parent_world = scene.getWorldMatrix(entity->getParentId());
    }
  }
  return makeLightGizmoWorldMatchingMesh(unique_world, parent_world,
                                         LightGizmoKind::directional,
                                         sceneDrawnMeshIsCentimetre(scene),
                                         unique_local);
}

}  // namespace

CameraGizmoOverlay::~CameraGizmoOverlay() {
  shutdown();
}

void CameraGizmoOverlay::initialize(const OverlayResources& res,
                                    SlangCompiler* compiler) {
  m_batch.initialize(res, compiler);
}

void CameraGizmoOverlay::shutdown() {
  m_batch.shutdown();
}

void CameraGizmoOverlay::begin_sync(OverlayResources& /*res*/,
                                    const OverlayState& /*state*/) {
  enabled_ = true;
}

void CameraGizmoOverlay::draw_screen(VkCommandBuffer cmd,
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

  const float aspect =
      static_cast<float>(state.viewport_width) /
      std::max(static_cast<float>(state.viewport_height), 1.0f);

  EditorSelectionSystem* selection =
      g_runtime_global_context.m_editor_selection.get();

  scene->tick(0.0f);

  EntityId sole_selected_camera{k_invalid_entity_id};
  if (selection != nullptr) {
    const eastl::vector<EntityId> selected_ids = selection->getSelectedIds();
    if (selected_ids.size() == 1 && scene->getCamera(selected_ids[0]) != nullptr) {
      sole_selected_camera = selected_ids[0];
    }
  }

  glm::vec4 frustum[6];
  extractFrustumPlanes(state.projection * state.view, frustum);

  scene->forEachCamera([&](EntityId entity_id, const CameraComponent& camera) {
    if (!scene->isActiveInHierarchy(entity_id)) {
      return;
    }

    const bool selected = selection != nullptr && selection->isSelected(entity_id);
    const glm::vec4 color = selected ? k_selected_color : k_muted_color;

    const float fov_rad = glm::radians(camera.vertical_fov_degrees);
    const CameraGizmoFrame frame = buildCameraGizmoFrameLocal(
        fov_rad, aspect, kCameraGizmoDisplayDistance);

    const glm::mat4 world = gizmoWorldForEntity(*scene, entity_id);
    const glm::vec3 origin = overlayGizmoWorldOrigin(world);
    const float radius =
        cameraGizmoLocalCullRadius(frame) * gizmoWorldScaleMax(world);
    if (!gizmoSphereVisible(origin, radius, frustum)) {
      return;
    }

    glm::vec3 corners[4];
    for (int i = 0; i < 4; ++i) {
      corners[i] = transformPoint(world, frame.corners[i]);
    }

    for (int i = 0; i < 4; ++i) {
      m_batch.push(GizmoLineDrawStyle::line, origin, corners[i], glm::vec3(0.0f),
                   color);
    }

    for (int i = 0; i < 4; ++i) {
      const int next = (i + 1) % 4;
      m_batch.push(GizmoLineDrawStyle::line, corners[i], corners[next],
                   glm::vec3(0.0f), color);
    }

    glm::vec3 tri[3];
    for (int i = 0; i < 3; ++i) {
      tri[i] = transformPoint(world, frame.up_triangle[i]);
    }
    m_batch.push(GizmoLineDrawStyle::triangle, tri[0], tri[1], tri[2], color);

    const glm::vec4 icon_color =
        selected ? glm::vec4(k_selected_color.x, k_selected_color.y,
                             k_selected_color.z, 1.0f)
                 : glm::vec4(0.0f, 0.0f, 0.0f, 0.95f);
    m_batch.push(GizmoLineDrawStyle::icon_billboard, origin, glm::vec3(0.0f),
                 glm::vec3(0.0f), icon_color);

    if (entity_id != sole_selected_camera) {
      return;
    }

    const float fov_rad_handles = glm::radians(camera.vertical_fov_degrees);
    const CameraGizmoFrame near_frame =
        buildCameraGizmoFrameLocal(fov_rad_handles, aspect, camera.near_clip);

    glm::vec3 near_corners[4];
    for (int i = 0; i < 4; ++i) {
      near_corners[i] = transformPoint(world, near_frame.corners[i]);
    }
    const glm::vec3 near_origin = transformPoint(world, near_frame.origin);

    for (int i = 0; i < 4; ++i) {
      const int next = (i + 1) % 4;
      m_batch.push(GizmoLineDrawStyle::line, near_corners[i], near_corners[next],
                   glm::vec3(0.0f), k_handle_color);
    }
    m_batch.push(GizmoLineDrawStyle::line, origin, near_origin, glm::vec3(0.0f),
                 k_handle_color);
  });

  m_batch.flush(cmd, state, k_line_width_px);
}

bool CameraGizmoOverlay::tryHandleMouseClick(const Vec2& window_position,
                                             EditorCamera& camera) {
  const std::optional<OverlayGizmoPickHit> hit = hitTest(window_position, camera);
  if (!hit.has_value()) {
    return false;
  }

  if (g_runtime_global_context.m_editor_selection) {
    g_runtime_global_context.m_editor_selection->setSelection(hit->entity_id);
  }
  if (g_runtime_global_context.m_viewport_pick) {
    g_runtime_global_context.m_viewport_pick->suppressNextLeftReleasePick();
  }
  return true;
}

std::optional<OverlayGizmoPickHit> CameraGizmoOverlay::hitTest(
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
  const float aspect = vp_w / vp_h;
  const glm::mat4 view = camera.getViewMatrix();
  const glm::mat4 proj = camera.getProjectionMatrix();

  EntityId best_entity{k_invalid_entity_id};
  float best_depth = -1e9f;

  scene->tick(0.0f);

  scene->forEachCamera([&](EntityId entity_id, const CameraComponent& cam) {
    if (!scene->isActiveInHierarchy(entity_id)) {
      return;
    }
    const float fov_rad = glm::radians(cam.vertical_fov_degrees);
    const CameraGizmoFrame frame =
        buildCameraGizmoFrameLocal(fov_rad, aspect, kCameraGizmoDisplayDistance);
    const glm::mat4 world = gizmoWorldForEntity(*scene, entity_id);
    const std::optional<float> hit_depth = hitTestCameraGizmoFrameViewportLocal(
        pointer, frame, world, view, proj, vp_w, vp_h);
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
