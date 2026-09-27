#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "EASTL/vector.h"

#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/mesh_asset.h"

namespace Blunder {

/// Fallback half-extent when a selected entity has no mesh/collider/CCT.
inline constexpr float kSelectionFocusEmptyHalfExtent = 0.25f;

inline bool isEntityOrDescendantOf(const SceneInstance& scene, EntityId root,
                                   EntityId candidate) {
  EntityId current = candidate;
  while (isValid(current)) {
    if (current == root) {
      return true;
    }
    const Entity* entity = scene.getEntity(current);
    if (entity == nullptr) {
      break;
    }
    current = entity->getParentId();
  }
  return false;
}

inline void expandAabbWithPoint(AABB& bounds, bool& any, const Vec3& point) {
  if (!any) {
    bounds.min = bounds.max = point;
    any = true;
  } else {
    bounds.expandToInclude(point);
  }
}

inline void expandAabbWithLocalAabb(AABB& bounds, bool& any, const AABB& local,
                                    const Mat4& world) {
  for (int corner = 0; corner < 8; ++corner) {
    const Vec3 local_corner((corner & 1) ? local.max.x : local.min.x,
                            (corner & 2) ? local.max.y : local.min.y,
                            (corner & 4) ? local.max.z : local.min.z);
    expandAabbWithPoint(bounds, any, Vec3(world * Vec4(local_corner, 1.0f)));
  }
}

inline float maxAbsScaleFromWorld(const Mat4& world) {
  const float sx = glm::length(Vec3(world[0]));
  const float sy = glm::length(Vec3(world[1]));
  const float sz = glm::length(Vec3(world[2]));
  return std::max({sx, sy, sz, 1e-4f});
}

inline void expandAabbWithCollider(AABB& bounds, bool& any,
                                   const ColliderComponent& collider,
                                   const Mat4& world) {
  switch (collider.shape) {
    case ColliderShapeKind::Sphere: {
      const Vec3 center = Vec3(world[3]);
      const float radius = collider.sphere_radius * maxAbsScaleFromWorld(world);
      expandAabbWithPoint(bounds, any, center - Vec3(radius));
      expandAabbWithPoint(bounds, any, center + Vec3(radius));
      break;
    }
    case ColliderShapeKind::Capsule: {
      const float half_h = colliderCapsuleHalfHeight(collider);
      const float r = collider.capsule_radius;
      const AABB local{Vec3(-r, -r, -(half_h + r)), Vec3(r, r, half_h + r)};
      expandAabbWithLocalAabb(bounds, any, local, world);
      break;
    }
    case ColliderShapeKind::TriangleMesh: {
      bool wrote = false;
      for (const ColliderTriangle& tri : collider.triangles) {
        expandAabbWithPoint(bounds, any, Vec3(world * Vec4(tri.v0, 1.0f)));
        expandAabbWithPoint(bounds, any, Vec3(world * Vec4(tri.v1, 1.0f)));
        expandAabbWithPoint(bounds, any, Vec3(world * Vec4(tri.v2, 1.0f)));
        wrote = true;
      }
      if (!wrote) {
        expandAabbWithPoint(bounds, any, Vec3(world[3]));
      }
      break;
    }
    case ColliderShapeKind::Box:
    default: {
      const Vec3 h = collider.box_half_extents;
      const AABB local{Vec3(-h.x, -h.y, -h.z), Vec3(h.x, h.y, h.z)};
      expandAabbWithLocalAabb(bounds, any, local, world);
      break;
    }
  }
}

inline void expandAabbWithCharacterController(
    AABB& bounds, bool& any, const CharacterControllerComponent& cct,
    const Mat4& world) {
  // Capsule CCT (engine main). Dog-walk sphere CCT still frames via mesh /
  // collider descendants on the same selection root.
  const float half_h = characterControllerHalfHeight(cct);
  const float r = cct.radius;
  const AABB local{Vec3(-r, -r, -(half_h + r)), Vec3(r, r, half_h + r)};
  expandAabbWithLocalAabb(bounds, any, local, world);
}

inline void expandAabbWithMeshRenderer(AABB& bounds, bool& any,
                                       const MeshRendererComponent& renderer,
                                       const Mat4& world) {
  if (!renderer.mesh) {
    return;
  }
  expandAabbWithLocalAabb(bounds, any, renderer.mesh->getLocalBounds(), world);
}

/// World AABB for one selected root: mesh / collider / CCT on the entity and
/// descendants. Transform-only entities get a small pad around the origin.
inline bool computeEntityFocusBounds(const SceneInstance& scene, EntityId root,
                                     AABB& out_bounds) {
  if (!isValid(root) || scene.getEntity(root) == nullptr) {
    return false;
  }

  AABB merged{};
  bool any = false;

  if (const MeshRendererComponent* renderer = scene.getMeshRenderer(root)) {
    expandAabbWithMeshRenderer(merged, any, *renderer, scene.getWorldMatrix(root));
  }
  if (const ColliderComponent* collider = scene.getCollider(root)) {
    expandAabbWithCollider(merged, any, *collider, scene.getWorldMatrix(root));
  }
  if (const CharacterControllerComponent* cct =
          scene.getCharacterController(root)) {
    expandAabbWithCharacterController(merged, any, *cct,
                                      scene.getWorldMatrix(root));
  }

  scene.forEachMeshRenderer(
      [&](EntityId id, const MeshRendererComponent& renderer) {
        if (id == root || !isEntityOrDescendantOf(scene, root, id)) {
          return;
        }
        if (!scene.isActiveInHierarchy(id)) {
          return;
        }
        expandAabbWithMeshRenderer(merged, any, renderer,
                                   scene.getWorldMatrix(id));
      });
  scene.forEachCollider([&](EntityId id, const ColliderComponent& collider) {
    if (id == root || !isEntityOrDescendantOf(scene, root, id)) {
      return;
    }
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    expandAabbWithCollider(merged, any, collider, scene.getWorldMatrix(id));
  });
  scene.forEachCharacterController(
      [&](EntityId id, const CharacterControllerComponent& cct) {
        if (id == root || !isEntityOrDescendantOf(scene, root, id)) {
          return;
        }
        if (!scene.isActiveInHierarchy(id)) {
          return;
        }
        expandAabbWithCharacterController(merged, any, cct,
                                          scene.getWorldMatrix(id));
      });

  if (!any) {
    const Vec3 origin = Vec3(scene.getWorldMatrix(root)[3]);
    const Vec3 pad(kSelectionFocusEmptyHalfExtent);
    merged.min = origin - pad;
    merged.max = origin + pad;
    any = true;
  }

  out_bounds = merged;
  return any;
}

/// Union of focus AABBs for the current editor selection.
inline bool computeSelectionFocusBounds(const SceneInstance& scene,
                                        const eastl::vector<EntityId>& selected,
                                        AABB& out_bounds) {
  AABB merged{};
  bool any = false;
  for (EntityId id : selected) {
    AABB entity_bounds{};
    if (!computeEntityFocusBounds(scene, id, entity_bounds)) {
      continue;
    }
    expandAabbWithPoint(merged, any, entity_bounds.min);
    expandAabbWithPoint(merged, any, entity_bounds.max);
  }
  if (!any) {
    return false;
  }
  out_bounds = merged;
  return true;
}

}  // namespace Blunder
