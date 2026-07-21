#include "runtime/function/scene/scene_serializer.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glm/gtc/quaternion.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset_registry/asset_registry.h"

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

/// Object-scoped key lookup: match `key` (including quotes) only at the current
/// object depth, not inside nested objects/arrays or string values. Requires
/// ':' after the key so value tokens like `"behaviours"` cannot bind.
const char* findObjectKey(const char* text, const char* limit, const char* key) {
  if (text == nullptr || limit == nullptr || key == nullptr || text >= limit) {
    return nullptr;
  }
  const size_t key_len = std::strlen(key);
  if (key_len == 0) {
    return nullptr;
  }

  const char* first = skipWhitespace(text);
  // Content spans (after '{') start already inside the object.
  int brace_depth = (first < limit && *first == '{') ? 0 : 1;
  int bracket_depth = 0;
  bool in_string = false;
  bool escape = false;

  for (const char* p = text; p < limit; ++p) {
    const char c = *p;
    if (in_string) {
      if (escape) {
        escape = false;
        continue;
      }
      if (c == '\\') {
        escape = true;
        continue;
      }
      if (c == '"') {
        in_string = false;
      }
      continue;
    }

    if (c == '"') {
      if (brace_depth == 1 && bracket_depth == 0 &&
          static_cast<size_t>(limit - p) >= key_len &&
          std::strncmp(p, key, key_len) == 0) {
        const char* after = skipWhitespace(p + key_len);
        if (after < limit && *after == ':') {
          return p;
        }
      }
      in_string = true;
      continue;
    }

    if (c == '{') {
      ++brace_depth;
    } else if (c == '}') {
      --brace_depth;
      if (brace_depth < 0) {
        return nullptr;
      }
    } else if (c == '[') {
      ++bracket_depth;
    } else if (c == ']') {
      --bracket_depth;
    }
  }
  return nullptr;
}

bool parseJsonString(const char* quote_start, const char* limit,
                     eastl::string& out_value, const char** out_after) {
  if (quote_start == nullptr || limit == nullptr || quote_start >= limit ||
      *quote_start != '"') {
    return false;
  }
  out_value.clear();
  const char* p = quote_start + 1;
  while (p < limit) {
    const char c = *p;
    if (c == '"') {
      *out_after = p + 1;
      return true;
    }
    if (c == '\\') {
      ++p;
      if (p >= limit) {
        return false;
      }
      switch (*p) {
        case '"':
          out_value.push_back('"');
          break;
        case '\\':
          out_value.push_back('\\');
          break;
        case '/':
          out_value.push_back('/');
          break;
        case 'b':
          out_value.push_back('\b');
          break;
        case 'f':
          out_value.push_back('\f');
          break;
        case 'n':
          out_value.push_back('\n');
          break;
        case 'r':
          out_value.push_back('\r');
          break;
        case 't':
          out_value.push_back('\t');
          break;
        default:
          out_value.push_back(*p);
          break;
      }
      ++p;
      continue;
    }
    out_value.push_back(c);
    ++p;
  }
  return false;
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
  const char* key_pos = findObjectKey(object_start, object_end, key);
  if (key_pos == nullptr) {
    return false;
  }
  const char* p = skipWhitespace(key_pos + std::strlen(key));
  if (p >= object_end || *p != ':') {
    return false;
  }
  ++p;
  p = skipWhitespace(p);
  const char* after = nullptr;
  if (!parseJsonString(p, object_end, out_value, &after)) {
    return false;
  }
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

const char* findObjectAfterKeyBounded(const char* text, const char* limit,
                                      const char* key, const char** out_end) {
  const char* key_pos = findObjectKey(text, limit, key);
  if (key_pos == nullptr) {
    return nullptr;
  }
  const char* p = skipWhitespace(key_pos + std::strlen(key));
  if (p >= limit || *p != ':') {
    return nullptr;
  }
  ++p;
  p = skipWhitespace(p);
  if (p >= limit || *p != '{') {
    return nullptr;
  }
  const char* brace = p;
  p = brace + 1;
  int depth = 1;
  while (p < limit && depth > 0) {
    if (*p == '{') {
      ++depth;
    } else if (*p == '}') {
      --depth;
    }
    ++p;
  }
  if (depth != 0) {
    return nullptr;
  }
  *out_end = p;
  return brace + 1;
}

const char* findArrayAfterKeyBounded(const char* text, const char* limit,
                                     const char* key, const char** out_end) {
  const char* key_pos = findObjectKey(text, limit, key);
  if (key_pos == nullptr) {
    return nullptr;
  }
  const char* p = skipWhitespace(key_pos + std::strlen(key));
  if (p >= limit || *p != ':') {
    return nullptr;
  }
  ++p;
  p = skipWhitespace(p);
  if (p >= limit || *p != '[') {
    return nullptr;
  }
  const char* bracket = p;
  p = bracket + 1;
  int depth = 1;
  while (p < limit && depth > 0) {
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

bool parseUint64Field(const char* object_start, const char* object_end,
                      const char* key, uint64_t& out_value) {
  const char* key_pos = findObjectKey(object_start, object_end, key);
  if (key_pos == nullptr) {
    return false;
  }
  const char* p = skipWhitespace(key_pos + std::strlen(key));
  if (p >= object_end || *p != ':') {
    return false;
  }
  ++p;
  p = skipWhitespace(p);
  if (p >= object_end) {
    return false;
  }
  char* after = nullptr;
  const unsigned long long parsed = std::strtoull(p, &after, 10);
  if (after == p || after > object_end) {
    return false;
  }
  out_value = static_cast<uint64_t>(parsed);
  return true;
}

bool parseBehaviourPropertyValue(const char* value_start, const char* limit,
                                 Variant& out_value, const char** out_after) {
  const char* p = skipWhitespace(value_start);
  if (p >= limit) {
    return false;
  }

  if (*p == '"') {
    eastl::string parsed;
    const char* after = nullptr;
    if (!parseJsonString(p, limit, parsed, &after)) {
      return false;
    }
    out_value = Variant(eastl::move(parsed));
    *out_after = after;
    return true;
  }

  if (limit - p >= 4 && std::strncmp(p, "true", 4) == 0 &&
      (p + 4 >= limit || !std::isalnum(static_cast<unsigned char>(p[4])))) {
    out_value = Variant(true);
    *out_after = p + 4;
    return true;
  }
  if (limit - p >= 5 && std::strncmp(p, "false", 5) == 0 &&
      (p + 5 >= limit || !std::isalnum(static_cast<unsigned char>(p[5])))) {
    out_value = Variant(false);
    *out_after = p + 5;
    return true;
  }

  char* after = nullptr;
  const double number = std::strtod(p, &after);
  if (after == p || after > limit) {
    return false;
  }
  // Prefer Int when the token has no fractional / exponent part.
  bool has_fraction = false;
  for (const char* c = p; c < after; ++c) {
    if (*c == '.' || *c == 'e' || *c == 'E') {
      has_fraction = true;
      break;
    }
  }
  if (has_fraction) {
    out_value = Variant(static_cast<float>(number));
  } else {
    out_value = Variant(static_cast<int64_t>(number));
  }
  *out_after = after;
  return true;
}

bool parseBehaviourProperties(const char* object_start, const char* object_end,
                              eastl::vector<SceneBehaviourProperty>& out_props) {
  out_props.clear();
  const char* props_end = nullptr;
  const char* props_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"properties\"", &props_end);
  if (props_content == nullptr) {
    return true;
  }

  const char* p = props_content;
  while (p < props_end - 1) {
    p = skipWhitespace(p);
    if (p >= props_end - 1 || *p == '}') {
      break;
    }
    if (*p != '"') {
      return false;
    }
    SceneBehaviourProperty prop;
    const char* after_key = nullptr;
    if (!parseJsonString(p, props_end, prop.key, &after_key)) {
      return false;
    }
    p = skipWhitespace(after_key);
    if (p >= props_end || *p != ':') {
      return false;
    }
    ++p;
    const char* after = nullptr;
    if (!parseBehaviourPropertyValue(p, props_end, prop.value, &after)) {
      return false;
    }
    out_props.push_back(eastl::move(prop));
    p = skipWhitespace(after);
    if (*p == ',') {
      ++p;
    }
  }
  return true;
}

bool parseBehaviourObject(const char* object_start, const char* object_end,
                          SceneBehaviourDeclaration& out_behaviour) {
  eastl::string type;
  if (!parseStringField(object_start, object_end, "\"type\"", type)) {
    return false;
  }
  out_behaviour.type = eastl::move(type);

  uint64_t id = 0;
  if (!parseUint64Field(object_start, object_end, "\"id\"", id) || id == 0) {
    return false;
  }
  out_behaviour.id = static_cast<BehaviourId>(id);

  if (!parseBehaviourProperties(object_start, object_end, out_behaviour.properties)) {
    return false;
  }
  return true;
}

bool parseBehavioursArray(const char* object_start, const char* object_end,
                          eastl::vector<SceneBehaviourDeclaration>& out_behaviours) {
  out_behaviours.clear();
  const char* array_end = nullptr;
  const char* array_content =
      findArrayAfterKeyBounded(object_start, object_end, "\"behaviours\"", &array_end);
  if (array_content == nullptr) {
    return true;
  }

  const char* p = array_content;
  while (p < array_end - 1) {
    p = skipWhitespace(p);
    if (p >= array_end - 1 || *p == ']') {
      break;
    }
    if (*p != '{') {
      ++p;
      continue;
    }

    const char* behaviour_start = p;
    int depth = 0;
    do {
      if (*p == '{') {
        ++depth;
      } else if (*p == '}') {
        --depth;
      }
      ++p;
    } while (p < array_end && depth > 0);

    if (depth != 0) {
      return false;
    }

    SceneBehaviourDeclaration behaviour;
    if (!parseBehaviourObject(behaviour_start, p, behaviour)) {
      LOG_WARN("[SceneSerializer] skipped malformed behaviour object");
    } else {
      out_behaviours.push_back(eastl::move(behaviour));
    }

    p = skipWhitespace(p);
    if (*p == ',') {
      ++p;
    }
  }
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

  eastl::string mesh_path;
  if (parseStringField(object_start, object_end, "\"mesh\"", mesh_path)) {
    out_entity.mesh_virtual_path = eastl::move(mesh_path);
  }

  if (!parseBehavioursArray(object_start, object_end, out_entity.behaviours)) {
    return false;
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

void appendJsonString(eastl::string& out, const eastl::string& value) {
  out.append("\"");
  for (size_t i = 0; i < value.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    switch (c) {
      case '"':
        out.append("\\\"");
        break;
      case '\\':
        out.append("\\\\");
        break;
      case '\b':
        out.append("\\b");
        break;
      case '\f':
        out.append("\\f");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x",
                        static_cast<unsigned>(c));
          out.append(buf);
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out.append("\"");
}

void appendBehaviourPropertyValue(eastl::string& out, const Variant& value) {
  char buffer[64];
  switch (value.getType()) {
    case VariantType::Bool:
      out.append(value.asBool() ? "true" : "false");
      break;
    case VariantType::Int:
      std::snprintf(buffer, sizeof(buffer), "%lld",
                    static_cast<long long>(value.asInt()));
      out.append(buffer);
      break;
    case VariantType::Float:
      std::snprintf(buffer, sizeof(buffer), "%.6g",
                    static_cast<double>(value.asFloat()));
      out.append(buffer);
      break;
    case VariantType::String:
      appendJsonString(out, value.asString());
      break;
    default:
      out.append("null");
      break;
  }
}

void appendBehaviourJson(eastl::string& out, const SceneBehaviourDeclaration& behaviour,
                         bool is_last) {
  out.append("        {\n");
  out.append("          \"type\": ");
  appendJsonString(out, behaviour.type);
  out.append(",\n          \"id\": ");
  char id_buffer[32];
  std::snprintf(id_buffer, sizeof(id_buffer), "%llu",
                static_cast<unsigned long long>(behaviour.id));
  out.append(id_buffer);

  if (!behaviour.properties.empty()) {
    out.append(",\n          \"properties\": {\n");
    for (size_t i = 0; i < behaviour.properties.size(); ++i) {
      const SceneBehaviourProperty& prop = behaviour.properties[i];
      out.append("            ");
      appendJsonString(out, prop.key);
      out.append(": ");
      appendBehaviourPropertyValue(out, prop.value);
      out.append(i + 1 == behaviour.properties.size() ? "\n" : ",\n");
    }
    out.append("          }");
  }

  out.append(is_last ? "\n        }\n" : "\n        },\n");
}

eastl::string meshReferenceForSerialize(const eastl::string& mesh_ref,
                                        const AssetRegistry* registry) {
  if (mesh_ref.empty() || isValidGuidFormat(mesh_ref) || registry == nullptr) {
    return mesh_ref;
  }
  const eastl::string guid = registry->findGuidForPath(mesh_ref);
  return guid.empty() ? mesh_ref : guid;
}

void migrateLegacyMeshReferences(Scene& scene, const AssetRegistry* registry) {
  if (registry == nullptr) {
    return;
  }
  for (SceneEntityDefinition& entity : scene.getEntities()) {
    if (entity.mesh_virtual_path.empty() ||
        isValidGuidFormat(entity.mesh_virtual_path)) {
      continue;
    }
    const eastl::string guid =
        registry->findGuidForPath(entity.mesh_virtual_path);
    if (!guid.empty()) {
      entity.mesh_virtual_path = guid;
    }
  }
}

void appendEntityJson(eastl::string& out, const SceneEntityDefinition& entity,
                      bool is_last, const AssetRegistry* registry) {
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

  if (!entity.mesh_virtual_path.empty()) {
    const eastl::string mesh_ref =
        meshReferenceForSerialize(entity.mesh_virtual_path, registry);
    out.append(",\n      \"mesh\": \"");
    out.append(mesh_ref);
    out.append("\"");
  }

  if (!entity.behaviours.empty()) {
    out.append(",\n      \"behaviours\": [\n");
    for (size_t i = 0; i < entity.behaviours.size(); ++i) {
      appendBehaviourJson(out, entity.behaviours[i],
                          i + 1 == entity.behaviours.size());
    }
    out.append("      ]");
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

bool SceneSerializer::deserialize(const eastl::string& json_text, Scene& out_scene,
                                  const AssetRegistry* registry) {
  out_scene.getEntities().clear();
  out_scene.getChildScenes().clear();
  out_scene.setGuid(eastl::string());

  if (json_text.empty()) {
    LOG_ERROR("[SceneSerializer] empty JSON text");
    return false;
  }

  eastl::string guid;
  if (parseStringField(json_text.c_str(), json_text.c_str() + json_text.size(),
                       "\"guid\"", guid) &&
      isValidGuidFormat(guid)) {
    out_scene.setGuid(eastl::move(guid));
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

  migrateLegacyMeshReferences(out_scene, registry);
  return true;
}

Quat SceneSerializer::rotationFromEulerDegrees(const Vec3& euler_degrees) {
  return rotationFromEulerDegreesImpl(euler_degrees);
}

Vec3 SceneSerializer::rotationToEulerDegrees(const Quat& rotation) {
  return rotationToEulerDegreesImpl(rotation);
}

bool SceneSerializer::serialize(const Scene& scene, eastl::string& out_json,
                                const AssetRegistry* registry) {
  out_json.clear();
  out_json.append("{\n");
  out_json.append("  \"type\": \"Scene\"");

  if (!scene.getGuid().empty()) {
    out_json.append(",\n  \"guid\": \"");
    out_json.append(scene.getGuid());
    out_json.append("\"");
  }

  const eastl::vector<SceneEntityDefinition>& entities = scene.getEntities();
  if (!entities.empty()) {
    out_json.append(",\n  \"entities\": [\n");
    for (size_t i = 0; i < entities.size(); ++i) {
      appendEntityJson(out_json, entities[i], i + 1 == entities.size(), registry);
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
