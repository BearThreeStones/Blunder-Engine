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
#include "runtime/core/object/skeleton_modifier_catalog.h"
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

bool skipJsonValueRaw(const char* start, const char* limit, const char** out_end) {
  const char* p = skipWhitespace(start);
  if (p >= limit || out_end == nullptr) {
    return false;
  }
  if (*p == '"') {
    bool escape = false;
    ++p;
    while (p < limit) {
      if (escape) {
        escape = false;
        ++p;
        continue;
      }
      if (*p == '\\') {
        escape = true;
        ++p;
        continue;
      }
      if (*p == '"') {
        *out_end = p + 1;
        return true;
      }
      ++p;
    }
    return false;
  }
  if (*p == '{' || *p == '[') {
    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool escape = false;
    bool started = false;
    for (const char* q = p; q < limit; ++q) {
      const char c = *q;
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
        in_string = true;
        continue;
      }
      if (c == '{') {
        ++brace_depth;
        started = true;
      } else if (c == '}') {
        --brace_depth;
      } else if (c == '[') {
        ++bracket_depth;
        started = true;
      } else if (c == ']') {
        --bracket_depth;
      }
      if (started && brace_depth == 0 && bracket_depth == 0) {
        *out_end = q + 1;
        return true;
      }
    }
    return false;
  }
  if (p + 4 <= limit && std::strncmp(p, "true", 4) == 0) {
    *out_end = p + 4;
    return true;
  }
  if (p + 5 <= limit && std::strncmp(p, "false", 5) == 0) {
    *out_end = p + 5;
    return true;
  }
  if (p + 4 <= limit && std::strncmp(p, "null", 4) == 0) {
    *out_end = p + 4;
    return true;
  }
  if (*p == '-' || std::isdigit(static_cast<unsigned char>(*p))) {
    const char* q = p;
    if (*q == '-') {
      ++q;
    }
    const char* num_start = q;
    while (q < limit &&
           (std::isdigit(static_cast<unsigned char>(*q)) || *q == '.' ||
            *q == 'e' || *q == 'E' || *q == '+' || *q == '-')) {
      ++q;
    }
    if (q == num_start) {
      return false;
    }
    *out_end = q;
    return true;
  }
  return false;
}

bool collectModifierExtraFields(const char* object_start, const char* object_end,
                                eastl::vector<SkeletonModifierExtraField>& out) {
  out.clear();
  const char* p = skipWhitespace(object_start);
  if (p >= object_end || *p != '{') {
    return false;
  }
  ++p;
  while (p < object_end) {
    p = skipWhitespace(p);
    if (p >= object_end) {
      return false;
    }
    if (*p == '}') {
      return true;
    }
    if (*p != '"') {
      return false;
    }
    eastl::string key;
    const char* after_key = nullptr;
    if (!parseJsonString(p, object_end, key, &after_key)) {
      return false;
    }
    p = skipWhitespace(after_key);
    if (p >= object_end || *p != ':') {
      return false;
    }
    ++p;
    p = skipWhitespace(p);
    const char* value_end = nullptr;
    if (!skipJsonValueRaw(p, object_end, &value_end)) {
      return false;
    }
    if (key != "type") {
      SkeletonModifierExtraField field;
      field.key = eastl::move(key);
      field.json_value.assign(p, static_cast<size_t>(value_end - p));
      out.push_back(eastl::move(field));
    }
    p = skipWhitespace(value_end);
    if (p < object_end && *p == ',') {
      ++p;
    }
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

bool parseStringArrayField(const char* object_start, const char* object_end,
                           const char* key,
                           eastl::vector<eastl::string>& out_values) {
  const char* array_end = nullptr;
  const char* array_content =
      findArrayAfterKeyBounded(object_start, object_end, key, &array_end);
  if (array_content == nullptr) {
    return false;
  }

  out_values.clear();
  const char* p = array_content;
  while (p < array_end - 1) {
    p = skipWhitespace(p);
    if (p >= array_end - 1 || *p == ']') {
      break;
    }
    if (*p != '"') {
      ++p;
      continue;
    }

    eastl::string value;
    const char* after = nullptr;
    if (!parseJsonString(p, array_end, value, &after)) {
      return false;
    }
    if (!value.empty()) {
      out_values.push_back(eastl::move(value));
    }
    p = after;
    p = skipWhitespace(p);
    if (p < array_end && *p == ',') {
      ++p;
    }
  }
  return true;
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

bool parseFloatField(const char* object_start, const char* object_end,
                     const char* key, float& out_value) {
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
  const float parsed = std::strtof(p, &after);
  if (after == p || after > object_end) {
    return false;
  }
  out_value = parsed;
  return true;
}

bool parseBoolField(const char* object_start, const char* object_end,
                    const char* key, bool& out_value) {
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
  if (object_end - p >= 4 && std::strncmp(p, "true", 4) == 0 &&
      (p + 4 >= object_end || !std::isalnum(static_cast<unsigned char>(p[4])))) {
    out_value = true;
    return true;
  }
  if (object_end - p >= 5 && std::strncmp(p, "false", 5) == 0 &&
      (p + 5 >= object_end || !std::isalnum(static_cast<unsigned char>(p[5])))) {
    out_value = false;
    return true;
  }
  return false;
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

bool parseStringStringMapObject(
    const char* object_start, const char* object_end,
    eastl::vector<SceneEntityDefinition::AnimationClipBinding>& out_bindings) {
  out_bindings.clear();
  const char* p = object_start;
  while (p < object_end - 1) {
    p = skipWhitespace(p);
    if (p >= object_end - 1 || *p == '}') {
      break;
    }
    if (*p != '"') {
      return false;
    }
    SceneEntityDefinition::AnimationClipBinding binding;
    const char* after_key = nullptr;
    if (!parseJsonString(p, object_end, binding.name, &after_key)) {
      return false;
    }
    p = skipWhitespace(after_key);
    if (p >= object_end || *p != ':') {
      return false;
    }
    ++p;
    p = skipWhitespace(p);
    const char* after_value = nullptr;
    if (!parseJsonString(p, object_end, binding.guid, &after_value)) {
      return false;
    }
    // Discard dual-empty drafts; keep half-filled for repair.
    if (!(binding.name.empty() && binding.guid.empty())) {
      out_bindings.push_back(eastl::move(binding));
    }
    p = skipWhitespace(after_value);
    if (p < object_end && *p == ',') {
      ++p;
    }
  }
  return true;
}

void rebuildAnimationClipGuidsFromPlayerMap(SceneEntityDefinition& entity) {
  if (entity.animation_player_clips.empty()) {
    return;
  }
  entity.animation_clip_guids.clear();
  for (const SceneEntityDefinition::AnimationClipBinding& binding :
       entity.animation_player_clips) {
    if (binding.guid.empty()) {
      continue;
    }
    bool duplicate = false;
    for (const eastl::string& existing : entity.animation_clip_guids) {
      if (existing == binding.guid) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      entity.animation_clip_guids.push_back(binding.guid);
    }
  }
}

eastl::vector<eastl::string> animationClipGuidsForSerialize(
    const SceneEntityDefinition& entity) {
  eastl::vector<eastl::string> guids;
  if (!entity.animation_player_clips.empty()) {
    for (const SceneEntityDefinition::AnimationClipBinding& binding :
         entity.animation_player_clips) {
      if (binding.guid.empty()) {
        continue;
      }
      bool duplicate = false;
      for (const eastl::string& existing : guids) {
        if (existing == binding.guid) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        guids.push_back(binding.guid);
      }
    }
    return guids;
  }
  return entity.animation_clip_guids;
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

bool parseSkeletonModifierObject(const char* object_start,
                                 const char* object_end,
                                 SceneSkeletonModifierDef& out_modifier) {
  eastl::string type;
  if (!parseStringField(object_start, object_end, "\"type\"", type)) {
    return false;
  }
  out_modifier.type = eastl::move(type);

  bool enabled = true;
  if (parseBoolField(object_start, object_end, "\"enabled\"", enabled)) {
    out_modifier.enabled = enabled;
  }

  eastl::string bone_name;
  if (parseStringField(object_start, object_end, "\"boneName\"", bone_name)) {
    out_modifier.bone_name = eastl::move(bone_name);
  }

  float open_amount = 0.0f;
  if (parseFloatField(object_start, object_end, "\"openAmount\"", open_amount)) {
    out_modifier.open_amount = open_amount;
  }

  bool attach_driven = false;
  if (parseBoolField(object_start, object_end, "\"attachDriven\"",
                     attach_driven)) {
    out_modifier.attach_driven = attach_driven;
  }

  Vec3 target(0.0f);
  if (parseVec3Field(object_start, object_end, "\"target\"", target,
                     out_modifier.target)) {
    out_modifier.target = target;
  }

  eastl::string child_entity_name;
  if (parseStringField(object_start, object_end, "\"childEntity\"",
                       child_entity_name)) {
    out_modifier.child_entity_name = eastl::move(child_entity_name);
  }
  if (!SkeletonModifierCatalog::hasType(out_modifier.type.c_str())) {
    collectModifierExtraFields(object_start, object_end,
                               out_modifier.extra_fields);
  }
  return true;
}

bool parseSkeletonModifiersArray(
    const char* object_start, const char* object_end,
    eastl::vector<SceneSkeletonModifierDef>& out_modifiers) {
  out_modifiers.clear();
  const char* array_end = nullptr;
  const char* array_content = findArrayAfterKeyBounded(
      object_start, object_end, "\"skeletonModifiers\"", &array_end);
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

    const char* modifier_start = p;
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

    SceneSkeletonModifierDef modifier;
    if (!parseSkeletonModifierObject(modifier_start, p, modifier)) {
      LOG_WARN("[SceneSerializer] skipped malformed skeletonModifier object");
    } else {
      out_modifiers.push_back(eastl::move(modifier));
    }

    p = skipWhitespace(p);
    if (p < array_end && *p == ',') {
      ++p;
    }
  }
  return true;
}

bool parseCameraObject(const char* object_start, const char* object_end,
                       CameraComponent& out_camera) {
  parseFloatField(object_start, object_end, "\"verticalFovDegrees\"",
                  out_camera.vertical_fov_degrees);
  parseFloatField(object_start, object_end, "\"nearClip\"",
                  out_camera.near_clip);
  parseFloatField(object_start, object_end, "\"farClip\"",
                  out_camera.far_clip);
  parseBoolField(object_start, object_end, "\"isMain\"", out_camera.is_main);
  return true;
}

bool parseLightObject(const char* object_start, const char* object_end,
                      LightComponent& out_light,
                      eastl::vector<eastl::string>& out_linking_names) {
  eastl::string type;
  if (parseStringField(object_start, object_end, "\"type\"", type)) {
    LightType parsed = LightType::directional;
    if (lightTypeFromJson(type, parsed)) {
      out_light.type = parsed;
    }
  }

  parseVec3Field(object_start, object_end, "\"color\"", out_light.color,
                 Vec3(1.0f, 1.0f, 1.0f));
  parseFloatField(object_start, object_end, "\"intensity\"", out_light.intensity);
  parseBoolField(object_start, object_end, "\"enabled\"", out_light.enabled);

  eastl::string contribution;
  if (parseStringField(object_start, object_end, "\"contribution\"",
                       contribution)) {
    LightContribution parsed = LightContribution::illuminateAndShadows;
    if (lightContributionFromJson(contribution, parsed)) {
      out_light.contribution = parsed;
    }
  }

  parseFloatField(object_start, object_end, "\"range\"", out_light.range);
  parseFloatField(object_start, object_end, "\"innerConeDegrees\"",
                  out_light.inner_cone_degrees);
  parseFloatField(object_start, object_end, "\"outerConeDegrees\"",
                  out_light.outer_cone_degrees);
  parseFloatField(object_start, object_end, "\"width\"", out_light.width);
  parseFloatField(object_start, object_end, "\"height\"", out_light.height);
  parseStringArrayField(object_start, object_end, "\"linking\"",
                        out_linking_names);
  return true;
}

bool parseAnimationTreeObject(const char* object_start, const char* object_end,
                              SceneEntityDefinition& out_entity);

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

  eastl::vector<eastl::string> clip_guids;
  if (parseStringArrayField(object_start, object_end,
                            "\"animation_clip_guids\"", clip_guids)) {
    out_entity.animation_clip_guids = eastl::move(clip_guids);
  }

  parseBoolField(object_start, object_end, "\"hasSkeleton\"",
                 out_entity.has_skeleton);

  const char* player_end = nullptr;
  const char* player_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"animationPlayer\"",
                                &player_end);
  if (player_content != nullptr) {
    const char* clips_end = nullptr;
    const char* clips_content =
        findObjectAfterKeyBounded(player_content, player_end, "\"clips\"",
                                  &clips_end);
    if (clips_content != nullptr) {
      if (!parseStringStringMapObject(clips_content, clips_end,
                                      out_entity.animation_player_clips)) {
        return false;
      }
    }

    float time_scale = 1.0f;
    if (parseFloatField(player_content, player_end, "\"timeScale\"", time_scale)) {
      out_entity.animation_player_time_scale = time_scale;
    }

    eastl::string slot0;
    if (parseStringField(player_content, player_end, "\"slot0\"", slot0)) {
      out_entity.animation_player_slot0 = eastl::move(slot0);
    }

    eastl::string slot1;
    if (parseStringField(player_content, player_end, "\"slot1\"", slot1)) {
      out_entity.animation_player_slot1 = eastl::move(slot1);
    }

    float blend_weight = 0.0f;
    if (parseFloatField(player_content, player_end, "\"blendWeight\"",
                        blend_weight)) {
      out_entity.animation_player_blend_weight = blend_weight;
    }
  }

  const char* tree_end = nullptr;
  const char* tree_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"animationTree\"",
                                &tree_end);
  if (tree_content != nullptr) {
    if (!parseAnimationTreeObject(tree_content, tree_end, out_entity)) {
      return false;
    }
  }

  rebuildAnimationClipGuidsFromPlayerMap(out_entity);

  if (!parseBehavioursArray(object_start, object_end, out_entity.behaviours)) {
    return false;
  }

  if (!parseSkeletonModifiersArray(object_start, object_end,
                                   out_entity.skeleton_modifiers)) {
    return false;
  }

  const char* camera_end = nullptr;
  const char* camera_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"camera\"", &camera_end);
  if (camera_content != nullptr) {
    out_entity.has_camera = true;
    if (!parseCameraObject(camera_content, camera_end, out_entity.camera)) {
      return false;
    }
  }

  const char* light_end = nullptr;
  const char* light_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"light\"", &light_end);
  if (light_content != nullptr) {
    out_entity.has_light = true;
    if (!parseLightObject(light_content, light_end, out_entity.light,
                          out_entity.light_linking_names)) {
      return false;
    }
  }

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

bool legacyChildScenesArrayNonEmpty(const char* json_text) {
  const char* array_end = nullptr;
  const char* array_content =
      findArrayAfterKey(json_text, "\"childScenes\"", &array_end);
  if (array_content == nullptr) {
    return false;
  }
  const char* p = skipWhitespace(array_content);
  return p < array_end && *p != ']';
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

void appendSkeletonModifierJson(eastl::string& out,
                                const SceneSkeletonModifierDef& modifier,
                                bool is_last) {
  char buffer[128];
  out.append("        {\n");
  out.append("          \"type\": ");
  appendJsonString(out, modifier.type);

  if (!SkeletonModifierCatalog::hasType(modifier.type.c_str())) {
    for (const SkeletonModifierExtraField& field : modifier.extra_fields) {
      out.append(",\n          ");
      appendJsonString(out, field.key);
      out.append(": ");
      out.append(field.json_value);
    }
    out.append(is_last ? "\n        }\n" : "\n        },\n");
    return;
  }

  if (!modifier.enabled) {
    out.append(",\n          \"enabled\": false");
  }

  if (!modifier.bone_name.empty()) {
    out.append(",\n          \"boneName\": ");
    appendJsonString(out, modifier.bone_name);
  }

  if (modifier.type == "PaperMouth") {
    std::snprintf(buffer, sizeof(buffer), ",\n          \"openAmount\": %.6g",
                  static_cast<double>(modifier.open_amount));
    out.append(buffer);
    if (modifier.attach_driven) {
      out.append(",\n          \"attachDriven\": true");
    }
  } else if (modifier.type == "SkeletonLookAtModifier") {
    out.append(",\n          \"target\": ");
    appendFloat3(out, modifier.target);
  } else if (modifier.type == "SkeletonAttachModifier") {
    if (!modifier.child_entity_name.empty()) {
      out.append(",\n          \"childEntity\": ");
      appendJsonString(out, modifier.child_entity_name);
    }
  }

  out.append(is_last ? "\n        }\n" : "\n        },\n");
}

void appendSkeletonModifiersJson(eastl::string& out,
                                 const SceneEntityDefinition& entity) {
  if (entity.skeleton_modifiers.empty()) {
    return;
  }
  out.append(",\n      \"skeletonModifiers\": [\n");
  for (size_t i = 0; i < entity.skeleton_modifiers.size(); ++i) {
    appendSkeletonModifierJson(out, entity.skeleton_modifiers[i],
                               i + 1 == entity.skeleton_modifiers.size());
  }
  out.append("      ]");
}

void appendAnimationPlayerJson(eastl::string& out,
                               const SceneEntityDefinition& entity) {
  const bool has_clips = !entity.animation_player_clips.empty();
  const bool has_defaults =
      entity.animation_player_time_scale != 1.0f ||
      !entity.animation_player_slot0.empty() ||
      !entity.animation_player_slot1.empty() ||
      entity.animation_player_blend_weight != 0.0f;
  if (!has_clips && !has_defaults) {
    return;
  }

  out.append(",\n      \"animationPlayer\": {\n");
  bool need_comma = false;

  if (has_clips) {
    out.append("        \"clips\": {\n");
    for (size_t i = 0; i < entity.animation_player_clips.size(); ++i) {
      const SceneEntityDefinition::AnimationClipBinding& binding =
          entity.animation_player_clips[i];
      out.append("          ");
      appendJsonString(out, binding.name);
      out.append(": ");
      appendJsonString(out, binding.guid);
      out.append(i + 1 == entity.animation_player_clips.size() ? "\n" : ",\n");
    }
    out.append("        }");
    need_comma = true;
  }

  if (entity.animation_player_time_scale != 1.0f) {
    if (need_comma) {
      out.append(",");
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "\n        \"timeScale\": %.6g",
                  static_cast<double>(entity.animation_player_time_scale));
    out.append(buffer);
    need_comma = true;
  }

  if (!entity.animation_player_slot0.empty()) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n        \"slot0\": ");
    appendJsonString(out, entity.animation_player_slot0);
    need_comma = true;
  }

  if (!entity.animation_player_slot1.empty()) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n        \"slot1\": ");
    appendJsonString(out, entity.animation_player_slot1);
    need_comma = true;
  }

  if (entity.animation_player_blend_weight != 0.0f) {
    if (need_comma) {
      out.append(",");
    }
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "\n        \"blendWeight\": %.6g",
                  static_cast<double>(entity.animation_player_blend_weight));
    out.append(buffer);
  }

  out.append("\n      }");
}

bool entityHasAnimationTreeTopology(const SceneEntityDefinition& entity) {
  return entity.has_animation_tree || entity.animation_tree_active ||
         !entity.animation_tree_current_state.empty() ||
         !entity.animation_tree_base_blend_space_node.empty() ||
         !entity.animation_tree_add2_clip.empty() ||
         !entity.animation_tree_oneshot_clip.empty() ||
         entity.animation_tree_add2_weight != 0.0f ||
         !entity.animation_tree_blend_spaces.empty() ||
         !entity.animation_tree_states.empty();
}

void appendAnimationTreeBlendSpacePointsJson(
    eastl::string& out,
    const eastl::vector<SceneEntityDefinition::AnimationTreeBlendSpacePointDef>&
        points) {
  out.append("          \"points\": [\n");
  for (size_t i = 0; i < points.size(); ++i) {
    const SceneEntityDefinition::AnimationTreeBlendSpacePointDef& point =
        points[i];
    out.append("            {\n");
    out.append("              \"clip\": ");
    appendJsonString(out, point.clip_name);
    out.append(",\n              \"scalar\": ");
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6g",
                  static_cast<double>(point.scalar));
    out.append(buffer);
    out.append(i + 1 == points.size() ? "\n            }\n" : "\n            },\n");
  }
  out.append("          ]");
}

void appendAnimationTreeBlendSpacesJson(
    eastl::string& out,
    const eastl::vector<SceneEntityDefinition::AnimationTreeBlendSpaceDef>&
        blend_spaces) {
  out.append("        \"blendSpaces\": {\n");
  for (size_t i = 0; i < blend_spaces.size(); ++i) {
    const SceneEntityDefinition::AnimationTreeBlendSpaceDef& space =
        blend_spaces[i];
    out.append("          ");
    appendJsonString(out, space.node_name);
    out.append(": {\n");
    if (space.scalar != 0.0f) {
      char buffer[64];
      std::snprintf(buffer, sizeof(buffer), "            \"scalar\": %.6g,\n",
                    static_cast<double>(space.scalar));
      out.append(buffer);
    }
    appendAnimationTreeBlendSpacePointsJson(out, space.points);
    out.append(i + 1 == blend_spaces.size() ? "\n          }\n" : "\n          },\n");
  }
  out.append("        }");
}

void appendAnimationTreeStatesJson(
    eastl::string& out,
    const eastl::vector<SceneEntityDefinition::AnimationTreeStateDef>& states) {
  out.append("        \"states\": {\n");
  for (size_t i = 0; i < states.size(); ++i) {
    const SceneEntityDefinition::AnimationTreeStateDef& state = states[i];
    out.append("          ");
    appendJsonString(out, state.name);
    out.append(": {\n");
    out.append("            \"kind\": ");
    appendJsonString(out, state.kind);
    if (state.kind == "blendSpace1D") {
      out.append(",\n            \"blendSpaceNode\": ");
      appendJsonString(out, state.blend_space_node);
    } else {
      out.append(",\n            \"clip\": ");
      appendJsonString(out, state.clip_name);
    }
    out.append(i + 1 == states.size() ? "\n          }\n" : "\n          },\n");
  }
  out.append("        }");
}

void appendAnimationTreeJson(eastl::string& out,
                             const SceneEntityDefinition& entity) {
  if (!entityHasAnimationTreeTopology(entity)) {
    return;
  }

  out.append(",\n      \"animationTree\": {\n");
  bool need_comma = false;

  if (entity.animation_tree_active) {
    out.append("        \"active\": true");
    need_comma = true;
  }

  if (!entity.animation_tree_current_state.empty()) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n        \"currentState\": ");
    appendJsonString(out, entity.animation_tree_current_state);
    need_comma = true;
  }

  if (!entity.animation_tree_base_blend_space_node.empty()) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n        \"baseBlendSpaceNode\": ");
    appendJsonString(out, entity.animation_tree_base_blend_space_node);
    need_comma = true;
  }

  if (!entity.animation_tree_blend_spaces.empty()) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n");
    appendAnimationTreeBlendSpacesJson(out, entity.animation_tree_blend_spaces);
    need_comma = true;
  }

  if (!entity.animation_tree_states.empty()) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n");
    appendAnimationTreeStatesJson(out, entity.animation_tree_states);
    need_comma = true;
  }

  if (!entity.animation_tree_add2_clip.empty() ||
      entity.animation_tree_add2_weight != 0.0f) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n        \"add2\": {\n");
    out.append("          \"clip\": ");
    appendJsonString(out, entity.animation_tree_add2_clip);
    if (entity.animation_tree_add2_weight != 0.0f) {
      char buffer[64];
      std::snprintf(buffer, sizeof(buffer), ",\n          \"weight\": %.6g",
                    static_cast<double>(entity.animation_tree_add2_weight));
      out.append(buffer);
    }
    out.append("\n        }");
    need_comma = true;
  }

  if (!entity.animation_tree_oneshot_clip.empty()) {
    if (need_comma) {
      out.append(",");
    }
    out.append("\n        \"oneShotClip\": ");
    appendJsonString(out, entity.animation_tree_oneshot_clip);
  }

  out.append("\n      }");
}

bool parseAnimationTreeBlendSpacePoints(
    const char* array_start, const char* array_end,
    eastl::vector<SceneEntityDefinition::AnimationTreeBlendSpacePointDef>&
        out_points) {
  out_points.clear();
  const char* p = array_start;
  while (p < array_end - 1) {
    p = skipWhitespace(p);
    if (p >= array_end - 1 || *p == ']') {
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

    if (depth != 0) {
      return false;
    }

    SceneEntityDefinition::AnimationTreeBlendSpacePointDef point;
    eastl::string clip_name;
    if (!parseStringField(object_start, p, "\"clip\"", clip_name)) {
      return false;
    }
    point.clip_name = eastl::move(clip_name);
    float scalar = 0.0f;
    parseFloatField(object_start, p, "\"scalar\"", scalar);
    point.scalar = scalar;
    out_points.push_back(eastl::move(point));

    p = skipWhitespace(p);
    if (p < array_end && *p == ',') {
      ++p;
    }
  }
  return true;
}

bool parseAnimationTreeBlendSpacesObject(
    const char* object_start, const char* object_end,
    eastl::vector<SceneEntityDefinition::AnimationTreeBlendSpaceDef>&
        out_blend_spaces) {
  out_blend_spaces.clear();
  const char* p = object_start;
  while (p < object_end - 1) {
    p = skipWhitespace(p);
    if (p >= object_end - 1 || *p == '}') {
      break;
    }
    if (*p != '"') {
      return false;
    }

    SceneEntityDefinition::AnimationTreeBlendSpaceDef space;
    const char* after_key = nullptr;
    if (!parseJsonString(p, object_end, space.node_name, &after_key)) {
      return false;
    }
    p = skipWhitespace(after_key);
    if (p >= object_end || *p != ':') {
      return false;
    }
    ++p;
    p = skipWhitespace(p);
    if (p >= object_end || *p != '{') {
      return false;
    }

    const char* node_start = p;
    int depth = 0;
    do {
      if (*p == '{') {
        ++depth;
      } else if (*p == '}') {
        --depth;
      }
      ++p;
    } while (p < object_end && depth > 0);
    if (depth != 0) {
      return false;
    }

    float scalar = 0.0f;
    parseFloatField(node_start, p, "\"scalar\"", scalar);
    space.scalar = scalar;

    const char* points_end = nullptr;
    const char* points_content =
        findArrayAfterKeyBounded(node_start, p, "\"points\"", &points_end);
    if (points_content != nullptr) {
      if (!parseAnimationTreeBlendSpacePoints(points_content, points_end,
                                              space.points)) {
        return false;
      }
    }

    if (!space.node_name.empty()) {
      out_blend_spaces.push_back(eastl::move(space));
    }

    p = skipWhitespace(p);
    if (p < object_end && *p == ',') {
      ++p;
    }
  }
  return true;
}

bool parseAnimationTreeStateObject(const char* object_start,
                                   const char* object_end,
                                   SceneEntityDefinition::AnimationTreeStateDef&
                                       out_state) {
  eastl::string kind;
  if (!parseStringField(object_start, object_end, "\"kind\"", kind)) {
    return false;
  }
  out_state.kind = eastl::move(kind);

  if (out_state.kind == "blendSpace1D") {
    eastl::string blend_space_node;
    if (!parseStringField(object_start, object_end, "\"blendSpaceNode\"",
                          blend_space_node)) {
      return false;
    }
    out_state.blend_space_node = eastl::move(blend_space_node);
  } else {
    eastl::string clip_name;
    if (!parseStringField(object_start, object_end, "\"clip\"", clip_name)) {
      return false;
    }
    out_state.clip_name = eastl::move(clip_name);
  }
  return true;
}

bool parseAnimationTreeStatesObject(
    const char* object_start, const char* object_end,
    eastl::vector<SceneEntityDefinition::AnimationTreeStateDef>& out_states) {
  out_states.clear();
  const char* p = object_start;
  while (p < object_end - 1) {
    p = skipWhitespace(p);
    if (p >= object_end - 1 || *p == '}') {
      break;
    }
    if (*p != '"') {
      return false;
    }

    SceneEntityDefinition::AnimationTreeStateDef state;
    const char* after_key = nullptr;
    if (!parseJsonString(p, object_end, state.name, &after_key)) {
      return false;
    }
    p = skipWhitespace(after_key);
    if (p >= object_end || *p != ':') {
      return false;
    }
    ++p;
    p = skipWhitespace(p);
    if (p >= object_end || *p != '{') {
      return false;
    }

    const char* state_start = p;
    int depth = 0;
    do {
      if (*p == '{') {
        ++depth;
      } else if (*p == '}') {
        --depth;
      }
      ++p;
    } while (p < object_end && depth > 0);
    if (depth != 0) {
      return false;
    }

    if (!parseAnimationTreeStateObject(state_start, p, state)) {
      return false;
    }
    if (!state.name.empty()) {
      out_states.push_back(eastl::move(state));
    }

    p = skipWhitespace(p);
    if (p < object_end && *p == ',') {
      ++p;
    }
  }
  return true;
}

bool parseAnimationTreeObject(const char* object_start, const char* object_end,
                              SceneEntityDefinition& out_entity) {
  out_entity.has_animation_tree = true;

  bool active = false;
  if (parseBoolField(object_start, object_end, "\"active\"", active)) {
    out_entity.animation_tree_active = active;
  }

  eastl::string current_state;
  if (parseStringField(object_start, object_end, "\"currentState\"",
                       current_state)) {
    out_entity.animation_tree_current_state = eastl::move(current_state);
  }

  eastl::string base_blend_space_node;
  if (parseStringField(object_start, object_end, "\"baseBlendSpaceNode\"",
                       base_blend_space_node)) {
    out_entity.animation_tree_base_blend_space_node =
        eastl::move(base_blend_space_node);
  }

  const char* blend_spaces_end = nullptr;
  const char* blend_spaces_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"blendSpaces\"",
                                &blend_spaces_end);
  if (blend_spaces_content != nullptr) {
    if (!parseAnimationTreeBlendSpacesObject(
            blend_spaces_content, blend_spaces_end,
            out_entity.animation_tree_blend_spaces)) {
      return false;
    }
  }

  const char* states_end = nullptr;
  const char* states_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"states\"",
                                &states_end);
  if (states_content != nullptr) {
    if (!parseAnimationTreeStatesObject(states_content, states_end,
                                        out_entity.animation_tree_states)) {
      return false;
    }
  }

  const char* add2_end = nullptr;
  const char* add2_content =
      findObjectAfterKeyBounded(object_start, object_end, "\"add2\"", &add2_end);
  if (add2_content != nullptr) {
    eastl::string add2_clip;
    if (parseStringField(add2_content, add2_end, "\"clip\"", add2_clip)) {
      out_entity.animation_tree_add2_clip = eastl::move(add2_clip);
    }
    float add2_weight = 0.0f;
    if (parseFloatField(add2_content, add2_end, "\"weight\"", add2_weight)) {
      out_entity.animation_tree_add2_weight = add2_weight;
    }
  }

  eastl::string one_shot_clip;
  if (parseStringField(object_start, object_end, "\"oneShotClip\"",
                       one_shot_clip)) {
    out_entity.animation_tree_oneshot_clip = eastl::move(one_shot_clip);
  }

  return true;
}

void appendCameraJson(eastl::string& out, const CameraComponent& camera) {
  char buffer[128];
  out.append(",\n      \"camera\": {\n");
  std::snprintf(buffer, sizeof(buffer), "        \"verticalFovDegrees\": %.6g,\n",
                static_cast<double>(camera.vertical_fov_degrees));
  out.append(buffer);
  std::snprintf(buffer, sizeof(buffer), "        \"nearClip\": %.6g,\n",
                static_cast<double>(camera.near_clip));
  out.append(buffer);
  std::snprintf(buffer, sizeof(buffer), "        \"farClip\": %.6g,\n",
                static_cast<double>(camera.far_clip));
  out.append(buffer);
  out.append("        \"isMain\": ");
  out.append(camera.is_main ? "true" : "false");
  out.append("\n      }");
}

void appendLightJson(eastl::string& out, const LightComponent& light,
                     const eastl::vector<eastl::string>& linking_names) {
  char buffer[160];
  out.append(",\n      \"light\": {\n");
  out.append("        \"type\": \"");
  out.append(lightTypeToJson(light.type));
  out.append("\",\n        \"color\": ");
  appendFloat3(out, light.color);
  std::snprintf(buffer, sizeof(buffer), ",\n        \"intensity\": %.6g,\n",
                static_cast<double>(light.intensity));
  out.append(buffer);
  out.append("        \"enabled\": ");
  out.append(light.enabled ? "true" : "false");
  out.append(",\n        \"contribution\": \"");
  out.append(lightContributionToJson(light.contribution));
  std::snprintf(buffer, sizeof(buffer),
                "\",\n        \"range\": %.6g,\n        \"innerConeDegrees\": %.6g,\n"
                "        \"outerConeDegrees\": %.6g,\n        \"width\": %.6g,\n"
                "        \"height\": %.6g,\n        \"linking\": [\n",
                static_cast<double>(light.range),
                static_cast<double>(light.inner_cone_degrees),
                static_cast<double>(light.outer_cone_degrees),
                static_cast<double>(light.width),
                static_cast<double>(light.height));
  out.append(buffer);
  for (size_t i = 0; i < linking_names.size(); ++i) {
    out.append("          ");
    appendJsonString(out, linking_names[i]);
    out.append(i + 1 == linking_names.size() ? "\n" : ",\n");
  }
  out.append("        ]\n      }");
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

  if (entity.has_skeleton) {
    out.append(",\n      \"hasSkeleton\": true");
  }

  appendAnimationPlayerJson(out, entity);

  appendAnimationTreeJson(out, entity);

  const eastl::vector<eastl::string> clip_guids =
      animationClipGuidsForSerialize(entity);
  if (!clip_guids.empty()) {
    out.append(",\n      \"animation_clip_guids\": [\n");
    for (size_t i = 0; i < clip_guids.size(); ++i) {
      out.append("        \"");
      out.append(clip_guids[i]);
      out.append(i + 1 == clip_guids.size() ? "\"\n" : "\",\n");
    }
    out.append("      ]");
  }

  if (!entity.behaviours.empty()) {
    out.append(",\n      \"behaviours\": [\n");
    for (size_t i = 0; i < entity.behaviours.size(); ++i) {
      appendBehaviourJson(out, entity.behaviours[i],
                          i + 1 == entity.behaviours.size());
    }
    out.append("      ]");
  }

  appendSkeletonModifiersJson(out, entity);

  if (entity.has_camera) {
    appendCameraJson(out, entity.camera);
  }

  if (entity.has_light) {
    appendLightJson(out, entity.light, entity.light_linking_names);
  }

  out.append(is_last ? "\n    }\n" : "\n    },\n");
}

}  // namespace

bool SceneSerializer::deserialize(const eastl::string& json_text, Scene& out_scene,
                                  const AssetRegistry* registry) {
  out_scene.getEntities().clear();
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

  if (legacyChildScenesArrayNonEmpty(json_text.c_str())) {
    LOG_WARN(
        "[SceneSerializer] ignoring legacy childScenes (removed; see ADR 0030); "
        "field will be dropped on next Save");
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

  out_json.append("\n}\n");
  return true;
}

}  // namespace Blunder
