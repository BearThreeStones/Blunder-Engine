#pragma once

#include "runtime/function/scene/collider_component.h"

#include <cgltf.h>

#include <cstddef>

namespace Blunder {

/// Parse glTF extras JSON for blender-studio / Godot `collision_info`.
/// Returns true only when a usable triangle list is present (Static trimesh).
bool parseCollisionExtrasJson(const char* json, size_t length, ColliderComponent& out);

/// Extract triangle list from a glTF mesh primitive (POSITION + indices).
/// Vertices are remapped glTF Y-up → engine Z-up. Returns false when empty/unreadable.
bool extractColliderTrianglesFromPrimitive(const cgltf_primitive& primitive,
                                           eastl::vector<ColliderTriangle>& out_triangles);

/// Build a Static triangleMesh Collider from all triangle primitives on a mesh.
/// Returns false when no usable triangles (caller should skip / omit collision).
bool buildStaticTrimeshColliderFromMesh(const cgltf_mesh* mesh, ColliderComponent& out);

}  // namespace Blunder
