#pragma once

#include <algorithm>
#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "EASTL/vector.h"

#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/overlay/collision_gizmo_geometry.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/project/play_pose_preview.h"

namespace Blunder {

inline constexpr uint8_t kCollisionOverlayR = 40;
inline constexpr uint8_t kCollisionOverlayG = 220;
inline constexpr uint8_t kCollisionOverlayB = 255;

inline MeshPreviewCameraFrame defaultCollisionCaptureFraming() {
  MeshPreviewCameraFrame framing{};
  framing.eye = glm::vec3(0.0f, -18.0f, 14.0f);
  framing.target = glm::vec3(0.0f, 0.0f, 0.0f);
  framing.up = glm::vec3(0.0f, 0.0f, 1.0f);
  framing.vertical_fov_rad = glm::radians(45.0f);
  framing.ok = true;
  return framing;
}

inline void plotCollisionOverlayPixel(uint8_t* rgba, uint32_t width, uint32_t height,
                                      int x, int y) {
  if (rgba == nullptr || x < 0 || y < 0 || static_cast<uint32_t>(x) >= width ||
      static_cast<uint32_t>(y) >= height) {
    return;
  }
  uint8_t* pixel =
      rgba + (static_cast<size_t>(y) * static_cast<size_t>(width) +
              static_cast<size_t>(x)) *
                 4u;
  pixel[0] = kCollisionOverlayR;
  pixel[1] = kCollisionOverlayG;
  pixel[2] = kCollisionOverlayB;
  pixel[3] = 255;
}

inline void drawCollisionOverlayLine(uint8_t* rgba, uint32_t width, uint32_t height,
                                     int x0, int y0, int x1, int y1) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    plotCollisionOverlayPixel(rgba, width, height, x0, y0);
    plotCollisionOverlayPixel(rgba, width, height, x0 + 1, y0);
    plotCollisionOverlayPixel(rgba, width, height, x0, y0 + 1);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = err * 2;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

inline bool projectCollisionWorldToPixel(const glm::mat4& view_proj, const glm::vec3& world,
                                         uint32_t width, uint32_t height, int& out_x,
                                         int& out_y) {
  const glm::vec4 clip = view_proj * glm::vec4(world, 1.0f);
  if (!(clip.w > 1e-5f)) {
    return false;
  }
  const glm::vec3 ndc = glm::vec3(clip) / clip.w;
  if (ndc.z < -1.0f || ndc.z > 1.0f) {
    return false;
  }
  out_x = static_cast<int>((ndc.x * 0.5f + 0.5f) * static_cast<float>(width));
  out_y = static_cast<int>((1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height));
  return true;
}

inline bool collisionStillIsBlank(const uint8_t* rgba, uint32_t width, uint32_t height) {
  if (rgba == nullptr || width == 0 || height == 0) {
    return true;
  }
  const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  for (size_t i = 0; i < count; i += 4) {
    if (rgba[i] > 12 || rgba[i + 1] > 12 || rgba[i + 2] > 12) {
      return false;
    }
  }
  return true;
}

inline void fillCollisionStillBackground(eastl::vector<uint8_t>& rgba, uint32_t width,
                                         uint32_t height) {
  rgba.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
  for (size_t i = 0; i < rgba.size(); i += 4) {
    rgba[i] = 18;
    rgba[i + 1] = 22;
    rgba[i + 2] = 28;
    rgba[i + 3] = 255;
  }
}

inline void ensureCollisionStillBuffer(eastl::vector<uint8_t>& rgba, uint32_t& width,
                                       uint32_t& height) {
  const SceneStillExtent extent = captureStillExtent();
  if (width == 0 || height == 0) {
    width = extent.width;
    height = extent.height;
  }
  const size_t expected =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  if (rgba.size() != expected || collisionStillIsBlank(rgba.data(), width, height)) {
    fillCollisionStillBackground(rgba, width, height);
  }
}

inline uint32_t paintCollisionOverlayRgba(uint8_t* rgba, uint32_t width, uint32_t height,
                                          const MeshPreviewCameraFrame& framing,
                                          SceneInstance& scene,
                                          const PlayPoseOverlayMap* overlay) {
  if (rgba == nullptr || width == 0 || height == 0 || !framing.ok) {
    return 0;
  }
  scene.tick(0.0f);
  const float aspect =
      static_cast<float>(width) / std::max(static_cast<float>(height), 1.0f);
  const glm::mat4 view = glm::lookAt(framing.eye, framing.target, framing.up);
  const glm::mat4 proj =
      glm::perspective(framing.vertical_fov_rad, aspect, 0.1f, 1000.0f);
  const glm::mat4 view_proj = proj * view;
  uint32_t painted = 0;

  auto paint_segment = [&](const Mat4& world, const Vec3& a, const Vec3& b) {
    const glm::vec4 ha = world * glm::vec4(a.x, a.y, a.z, 1.0f);
    const glm::vec4 hb = world * glm::vec4(b.x, b.y, b.z, 1.0f);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!projectCollisionWorldToPixel(view_proj, glm::vec3(ha), width, height, x0,
                                      y0) ||
        !projectCollisionWorldToPixel(view_proj, glm::vec3(hb), width, height, x1,
                                      y1)) {
      return;
    }
    drawCollisionOverlayLine(rgba, width, height, x0, y0, x1, y1);
    ++painted;
  };

  scene.forEachCollider([&](EntityId entity_id, const ColliderComponent& collider) {
    if (!scene.isActiveInHierarchy(entity_id)) {
      return;
    }
    const Mat4 world = worldMatrixWithPlayPoseOverlay(scene, entity_id, overlay);
    forEachColliderWireSegment(collider, [&](const Vec3& a, const Vec3& b) {
      paint_segment(world, a, b);
    });
  });
  scene.forEachCharacterController(
      [&](EntityId entity_id, const CharacterControllerComponent& cct) {
        if (!scene.isActiveInHierarchy(entity_id)) {
          return;
        }
        const Mat4 world =
            worldMatrixWithPlayPoseOverlay(scene, entity_id, overlay);
        forEachCharacterControllerWireSegment(cct, [&](const Vec3& a, const Vec3& b) {
          paint_segment(world, a, b);
        });
      });
  return painted;
}

}  // namespace Blunder
