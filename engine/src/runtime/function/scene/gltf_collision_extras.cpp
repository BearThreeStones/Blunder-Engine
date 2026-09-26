#include "runtime/function/scene/gltf_collision_extras.h"

#include "runtime/core/math/coordinate_system.h"

#include "EASTL/vector.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace Blunder {
namespace {

const char* skipWs(const char* p, const char* end) {
  while (p < end && std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
  return p;
}

bool parseFloat(const char* p, const char* end, float& out, const char** after) {
  if (p >= end) {
    return false;
  }
  char* num_end = nullptr;
  const float value = std::strtof(p, &num_end);
  if (num_end == p || num_end > end) {
    return false;
  }
  out = value;
  *after = num_end;
  return true;
}

bool parseVec3Array(const char* object_start, const char* object_end, const char* key,
                    Vec3& out) {
  const char* key_pos = std::strstr(object_start, key);
  if (key_pos == nullptr || key_pos >= object_end) {
    return false;
  }
  const char* p = std::strchr(key_pos, '[');
  if (p == nullptr || p >= object_end) {
    return false;
  }
  ++p;
  p = skipWs(p, object_end);
  const char* after = nullptr;
  if (!parseFloat(p, object_end, out.x, &after)) {
    return false;
  }
  p = skipWs(after, object_end);
  if (p < object_end && *p == ',') {
    ++p;
  }
  p = skipWs(p, object_end);
  if (!parseFloat(p, object_end, out.y, &after)) {
    return false;
  }
  p = skipWs(after, object_end);
  if (p < object_end && *p == ',') {
    ++p;
  }
  p = skipWs(p, object_end);
  if (!parseFloat(p, object_end, out.z, &after)) {
    return false;
  }
  return true;
}

}  // namespace

bool parseCollisionExtrasJson(const char* json, size_t length, ColliderComponent& out) {
  if (json == nullptr || length == 0) {
    return false;
  }
  const char* end = json + length;
  const char* info = std::strstr(json, "\"collision_info\"");
  if (info == nullptr || info >= end) {
    return false;
  }
  const char* tris_key = std::strstr(info, "\"triangles\"");
  if (tris_key == nullptr || tris_key >= end) {
    return false;
  }
  const char* array_start = std::strchr(tris_key, '[');
  if (array_start == nullptr || array_start >= end) {
    return false;
  }
  int depth = 0;
  const char* array_end = array_start;
  do {
    if (*array_end == '[') {
      ++depth;
    } else if (*array_end == ']') {
      --depth;
    }
    ++array_end;
  } while (array_end < end && depth > 0);
  if (depth != 0) {
    return false;
  }

  ColliderComponent collider{};
  collider.shape = ColliderShapeKind::TriangleMesh;
  collider.body_kind = ColliderBodyKind::Static;

  const char* p = array_start + 1;
  while (p < array_end - 1) {
    p = skipWs(p, array_end);
    if (p >= array_end - 1 || *p == ']') {
      break;
    }
    if (*p != '{') {
      ++p;
      continue;
    }
    const char* obj_start = p;
    int obj_depth = 0;
    do {
      if (*p == '{') {
        ++obj_depth;
      } else if (*p == '}') {
        --obj_depth;
      }
      ++p;
    } while (p < array_end && obj_depth > 0);
    ColliderTriangle tri{};
    if (!parseVec3Array(obj_start, p, "\"v0\"", tri.v0) ||
        !parseVec3Array(obj_start, p, "\"v1\"", tri.v1) ||
        !parseVec3Array(obj_start, p, "\"v2\"", tri.v2)) {
      continue;
    }
    collider.triangles.push_back(tri);
  }

  if (collider.triangles.empty()) {
    return false;
  }
  sanitizeColliderComponent(collider);
  out = collider;
  return true;
}

bool extractColliderTrianglesFromPrimitive(const cgltf_primitive& primitive,
                                           eastl::vector<ColliderTriangle>& out_triangles) {
  if (primitive.type != cgltf_primitive_type_triangles) {
    return false;
  }
  const cgltf_accessor* position_accessor = nullptr;
  for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
    if (primitive.attributes[i].type == cgltf_attribute_type_position) {
      position_accessor = primitive.attributes[i].data;
      break;
    }
  }
  if (position_accessor == nullptr || position_accessor->count == 0) {
    return false;
  }

  eastl::vector<Vec3> positions(static_cast<size_t>(position_accessor->count));
  for (cgltf_size vertex_index = 0; vertex_index < position_accessor->count; ++vertex_index) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    if (!cgltf_accessor_read_float(position_accessor, vertex_index, position, 3)) {
      return false;
    }
    positions[static_cast<size_t>(vertex_index)] =
        transformPointGltfToEngine(Vec3(position[0], position[1], position[2]));
  }

  eastl::vector<uint32_t> indices;
  if (primitive.indices != nullptr) {
    indices.resize(static_cast<size_t>(primitive.indices->count));
    for (cgltf_size index = 0; index < primitive.indices->count; ++index) {
      const cgltf_size value = cgltf_accessor_read_index(primitive.indices, index);
      if (value > std::numeric_limits<uint32_t>::max() ||
          value >= positions.size()) {
        return false;
      }
      indices[static_cast<size_t>(index)] = static_cast<uint32_t>(value);
    }
  } else {
    indices.resize(positions.size());
    for (size_t index = 0; index < positions.size(); ++index) {
      indices[index] = static_cast<uint32_t>(index);
    }
  }

  if (indices.size() < 3 || (indices.size() % 3u) != 0u) {
    return false;
  }

  const size_t before = out_triangles.size();
  out_triangles.reserve(before + indices.size() / 3u);
  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    ColliderTriangle tri{};
    tri.v0 = positions[indices[i]];
    tri.v1 = positions[indices[i + 1]];
    tri.v2 = positions[indices[i + 2]];
    out_triangles.push_back(tri);
  }
  return out_triangles.size() > before;
}

bool buildStaticTrimeshColliderFromMesh(const cgltf_mesh* mesh, ColliderComponent& out) {
  if (mesh == nullptr || mesh->primitives_count == 0) {
    return false;
  }
  ColliderComponent collider{};
  collider.shape = ColliderShapeKind::TriangleMesh;
  collider.body_kind = ColliderBodyKind::Static;
  collider.layer = 1u;
  collider.mask = 0xFFFFFFFFu;
  for (cgltf_size prim_index = 0; prim_index < mesh->primitives_count; ++prim_index) {
    extractColliderTrianglesFromPrimitive(mesh->primitives[prim_index], collider.triangles);
  }
  if (collider.triangles.empty()) {
    return false;
  }
  sanitizeColliderComponent(collider);
  out = eastl::move(collider);
  return true;
}

}  // namespace Blunder
