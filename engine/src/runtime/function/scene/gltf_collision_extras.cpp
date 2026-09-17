#include "runtime/function/scene/gltf_collision_extras.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

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

}  // namespace Blunder
