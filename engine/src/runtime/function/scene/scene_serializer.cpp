#include "runtime/function/scene/scene_serializer.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glm/gtc/quaternion.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"

namespace Blunder {

namespace {

const char* skipWhitespace(const char* p) {
  while (*p != '\0' && std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
  return p;
}

const char* findKey(const char* text, const char* key) {
  const char* pos = std::strstr(text, key);
  return pos;
}

bool parseFloatArray(const char* start, const char* end, float* out, size_t count) {
  const char* p = start;
  for (size_t i = 0; i < count; ++i) {
    p = skipWhitespace(p);
    if (p >= end) {
      return false;
    }
    char* after = nullptr;
    out[i] = std::strtof(p, &after);
    if (after == p) {
      return false;
    }
    p = after;
    p = skipWhitespace(p);
    if (i + 1 < count) {
      if (*p != ',') {
        return false;
      }
      ++p;
    }
  }
  return true;
}

const char* findArrayAfterKey(const char* text, const char* key, const char** out_end) {
  const char* key_pos = findKey(text, key);
  if (key_pos == nullptr) {
    return nullptr;
  }
  const char* bracket = std::strchr(key_pos, '[');
  if (bracket == nullptr) {
    return nullptr;
  }
  const char* p = bracket + 1;
  int depth = 1;
  while (*p != '\0' && depth > 0) {
    if (*p == '[') {
      ++depth;
    } else if (*p == ']') {
      --depth;
    }
    ++p;
  }
  if (depth != 0) {
    return nullptr;
  }
  *out_end = p;
  return bracket + 1;
}

Quat rotationFromEulerDegreesImpl(const Vec3& euler_degrees) {
  const Vec3 radians(glm::radians(euler_degrees.x), glm::radians(euler_degrees.y),
                     glm::radians(euler_degrees.z));
  const Quat qx = glm::angleAxis(radians.x, kWorldRight);
  const Quat qy = glm::angleAxis(radians.y, kWorldForward);
  const Quat qz = glm::angleAxis(radians.z, kWorldUp);
  return qz * qy * qx;
}

Vec3 rotationToEulerDegreesImpl(const Quat& rotation) {
  const Quat q = glm::normalize(rotation);
  const float sy = 2.0f * (q.w * q.z + q.x * q.y);
  const float cy = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
  const float y = std::atan2(sy, cy);

  const float sx = 2.0f * (q.w * q.x + q.y * q.z);
  const float cx = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
  const float x = std::atan2(sx, cx);

  const float sz = 2.0f * (q.w * q.y + q.z * q.x);
  const float cz = 1.0f - 2.0f * (q.y * q.y + q.x * q.x);
  const float z = std::atan2(sz, cz);

  return glm::degrees(Vec3(x, y, z));
}

bool parseRotation(const char* object_start, const char* object_end, Quat& out_rotation) {
  const char* rotation_end = nullptr;
  const char* rotation_array =
      findArrayAfterKey(object_start, "\"rotation\"", &rotation_end);
  if (rotation_array == nullptr || rotation_end > object_end) {
    out_rotation = glm::identity<Quat>();
    return true;
  }

  float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const bool is_euler = findKey(object_start, "\"euler_degrees\"") != nullptr;
  const size_t component_count = is_euler ? 3u : 4u;
  if (!parseFloatArray(rotation_array, rotation_end - 1, values, component_count)) {
    return false;
  }

  if (is_euler) {
    out_rotation =
        rotationFromEulerDegreesImpl(Vec3(values[0], values[1], values[2]));
  } else {
    out_rotation = glm::normalize(Quat(values[3], values[0], values[1], values[2]));
  }
  return true;
}

bool parseStringField(const char* object_start, const char* object_end,
                      const char* key, eastl::string& out_value) {
  const char* key_pos = findKey(object_start, key);
  if (key_pos == nullptr || key_pos >= object_end) {
    return false;
  }
  const char* quote_start = std::strchr(key_pos + std::strlen(key), '"');
  if (quote_start == nullptr || quote_start >= object_end) {
    return false;
  }
  ++quote_start;
  const char* quote_end = std::strchr(quote_start, '"');
  if (quote_end == nullptr || quote_end > object_end) {
    return false;
  }
  out_value.assign(quote_start, static_cast<size_t>(quote_end - quote_start));
  return !out_value.empty();
}

bool parseVec3Field(const char* object_start, const char* object_end, const char* key,
                    Vec3& out_value, const Vec3& default_value) {
  out_value = default_value;
  const char* array_end = nullptr;
  const char* array_start = findArrayAfterKey(object_start, key, &array_end);
  if (array_start == nullptr || array_end > object_end) {
    return false;
  }
  float values[3] = {default_value.x, default_value.y, default_value.z};
  if (!parseFloatArray(array_start, array_end - 1, values, 3)) {
    return false;
  }
  out_value = Vec3(values[0], values[1], values[2]);
  return true;
}

bool parseEntityObject(const char* object_start, const char* object_end,
                       SceneEntityDefinition& out_entity) {
  eastl::string name;
  if (!parseStringField(object_start, object_end, "\"name\"", name)) {
    return false;
  }
  out_entity.name = eastl::move(name);

  parseVec3Field(object_start, object_end, "\"position\"", out_entity.position,
                 Vec3(0.0f));
  parseVec3Field(object_start, object_end, "\"scale\"", out_entity.scale,
                 Vec3(1.0f, 1.0f, 1.0f));
  parseRotation(object_start, object_end, out_entity.rotation);

  eastl::string parent_name;
  if (parseStringField(object_start, object_end, "\"parent\"", parent_name)) {
    out_entity.parent_name = eastl::move(parent_name);
  }

  return true;
}

bool parseChildSceneObject(const char* object_start, const char* object_end,
                           SceneChildReference& out_child) {
  if (!parseStringField(object_start, object_end, "\"scene\"", out_child.scene_virtual_path)) {
    return false;
  }

  eastl::string instance_name;
  if (parseStringField(object_start, object_end, "\"name\"", instance_name)) {
    out_child.instance_name = eastl::move(instance_name);
  } else {
    out_child.instance_name = out_child.scene_virtual_path;
  }

  parseVec3Field(object_start, object_end, "\"position\"", out_child.position, Vec3(0.0f));
  parseVec3Field(object_start, object_end, "\"scale\"", out_child.scale,
                 Vec3(1.0f, 1.0f, 1.0f));
  parseRotation(object_start, object_end, out_child.rotation);
  return true;
}

bool parseObjectArray(const char* json_text, const char* array_key,
                      bool (*parse_object)(const char*, const char*, void*),
                      void* context_push) {
  const char* array_end = nullptr;
  const char* array_content = findArrayAfterKey(json_text, array_key, &array_end);
  if (array_content == nullptr) {
    return true;
  }

  const char* p = array_content;
  while (p < array_end - 1) {
    p = skipWhitespace(p);
    if (p >= array_end - 1) {
      break;
    }
    if (*p == ']') {
      break;
    }
    if (*p != '{') {
      ++p;
      continue;
    }

    const char* object_start = p;
    int depth = 0;
    do {
      if (*p == '{') {
        ++depth;
      } else if (*p == '}') {
        --depth;
      }
      ++p;
    } while (p < array_end && depth > 0);

    const char* object_end = p;
    if (depth != 0) {
      return false;
    }

    if (!parse_object(object_start, object_end, context_push)) {
      return false;
    }

    p = skipWhitespace(p);
    if (*p == ',') {
      ++p;
    }
  }

  return true;
}

struct EntityParseContext {
  Scene* scene;
};

bool parseEntityCallback(const char* object_start, const char* object_end, void* ctx) {
  auto* context = static_cast<EntityParseContext*>(ctx);
  SceneEntityDefinition definition;
  if (!parseEntityObject(object_start, object_end, definition)) {
    LOG_WARN("[SceneSerializer] skipped malformed entity object");
    return true;
  }
  context->scene->getEntities().push_back(eastl::move(definition));
  return true;
}

struct ChildParseContext {
  Scene* scene;
};

bool parseChildCallback(const char* object_start, const char* object_end, void* ctx) {
  auto* context = static_cast<ChildParseContext*>(ctx);
  SceneChildReference child;
  if (!parseChildSceneObject(object_start, object_end, child)) {
    LOG_WARN("[SceneSerializer] skipped malformed childScenes object");
    return true;
  }
  context->scene->getChildScenes().push_back(eastl::move(child));
  return true;
}

void appendFloat3(eastl::string& out, const Vec3& v) {
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "[%.6g, %.6g, %.6g]", v.x, v.y, v.z);
  out.append(buffer);
}

void appendEntityJson(eastl::string& out, const SceneEntityDefinition& entity,
                      bool is_last) {
  out.append("    {\n");
  out.append("      \"name\": \"");
  out.append(entity.name);
  out.append("\",\n");

  out.append("      \"position\": ");
  appendFloat3(out, entity.position);
  out.append(",\n");

  const Vec3 euler = rotationToEulerDegreesImpl(entity.rotation);
  out.append("      \"rotation\": ");
  appendFloat3(out, euler);
  out.append(",\n");
  out.append("      \"rotationMode\": \"euler_degrees\"");

  if (entity.scale.x != 1.0f || entity.scale.y != 1.0f || entity.scale.z != 1.0f) {
    out.append(",\n      \"scale\": ");
    appendFloat3(out, entity.scale);
  }

  if (!entity.parent_name.empty()) {
    out.append(",\n      \"parent\": \"");
    out.append(entity.parent_name);
    out.append("\"");
  }

  out.append(is_last ? "\n    }\n" : "\n    },\n");
}

void appendChildSceneJson(eastl::string& out, const SceneChildReference& child,
                          bool is_last) {
  out.append("    {\n");
  out.append("      \"scene\": \"");
  out.append(child.scene_virtual_path);
  out.append("\",\n      \"name\": \"");
  out.append(child.instance_name);
  out.append("\",\n      \"position\": ");
  appendFloat3(out, child.position);
  out.append(",\n      \"rotation\": ");
  appendFloat3(out, rotationToEulerDegreesImpl(child.rotation));
  out.append(",\n      \"rotationMode\": \"euler_degrees\",\n      \"scale\": ");
  appendFloat3(out, child.scale);
  out.append(is_last ? "\n    }\n" : "\n    },\n");
}

}  // namespace

bool SceneSerializer::deserialize(const eastl::string& json_text, Scene& out_scene) {
  out_scene.getEntities().clear();
  out_scene.getChildScenes().clear();

  if (json_text.empty()) {
    LOG_ERROR("[SceneSerializer] empty JSON text");
    return false;
  }

  EntityParseContext entity_ctx{&out_scene};
  if (!parseObjectArray(json_text.c_str(), "\"entities\"", parseEntityCallback,
                        &entity_ctx)) {
    LOG_ERROR("[SceneSerializer] failed parsing entities array");
    return false;
  }

  ChildParseContext child_ctx{&out_scene};
  if (!parseObjectArray(json_text.c_str(), "\"childScenes\"", parseChildCallback,
                        &child_ctx)) {
    LOG_ERROR("[SceneSerializer] failed parsing childScenes array");
    return false;
  }

  return true;
}

Quat SceneSerializer::rotationFromEulerDegrees(const Vec3& euler_degrees) {
  return rotationFromEulerDegreesImpl(euler_degrees);
}

Vec3 SceneSerializer::rotationToEulerDegrees(const Quat& rotation) {
  return rotationToEulerDegreesImpl(rotation);
}

bool SceneSerializer::serialize(const Scene& scene, eastl::string& out_json) {
  out_json.clear();
  out_json.append("{\n");
  out_json.append("  \"type\": \"Scene\"");

  const eastl::vector<SceneEntityDefinition>& entities = scene.getEntities();
  if (!entities.empty()) {
    out_json.append(",\n  \"entities\": [\n");
    for (size_t i = 0; i < entities.size(); ++i) {
      appendEntityJson(out_json, entities[i], i + 1 == entities.size());
    }
    out_json.append("  ]");
  }

  const eastl::vector<SceneChildReference>& children = scene.getChildScenes();
  if (!children.empty()) {
    out_json.append(",\n  \"childScenes\": [\n");
    for (size_t i = 0; i < children.size(); ++i) {
      appendChildSceneJson(out_json, children[i], i + 1 == children.size());
    }
    out_json.append("  ]");
  }

  out_json.append("\n}\n");
  return true;
}

}  // namespace Blunder
