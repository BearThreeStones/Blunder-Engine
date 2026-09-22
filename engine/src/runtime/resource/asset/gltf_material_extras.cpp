#include "runtime/resource/asset/gltf_material_extras.h"

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <string>

namespace Blunder {
namespace {

const char* skipWs(const char* p, const char* end) {
  while (p < end && std::isspace(static_cast<unsigned char>(*p))) {
    ++p;
  }
  return p;
}

bool findJsonObjectAfterKey(const char* json, const char* end, const char* key,
                            const char*& obj_begin, const char*& obj_end) {
  const char* key_pos = std::strstr(json, key);
  if (key_pos == nullptr || key_pos >= end) {
    return false;
  }
  const char* brace = std::strchr(key_pos, '{');
  if (brace == nullptr || brace >= end) {
    return false;
  }
  int depth = 0;
  const char* p = brace;
  do {
    if (*p == '{') {
      ++depth;
    } else if (*p == '}') {
      --depth;
    }
    ++p;
  } while (p < end && depth > 0);
  if (depth != 0) {
    return false;
  }
  obj_begin = brace;
  obj_end = p;
  return true;
}

bool parseFloatAfterQuotedKey(const char* begin, const char* end, const char* key,
                              float& out) {
  if (begin == nullptr || end == nullptr || key == nullptr || begin >= end) {
    return false;
  }
  const size_t key_len = std::strlen(key);
  const char* p = begin;
  while (p < end) {
    const size_t remaining = static_cast<size_t>(end - p);
    if (remaining < key_len) {
      return false;
    }
    const char* found = std::strstr(p, key);
    if (found == nullptr || found >= end) {
      return false;
    }
    p = found + key_len;
    p = skipWs(p, end);
    if (p >= end || *p != ':') {
      continue;
    }
    ++p;
    p = skipWs(p, end);
    if (p >= end || *p == '"' || *p == '[' || *p == '{') {
      continue;
    }
    char* num_end = nullptr;
    const float value = std::strtof(p, &num_end);
    if (num_end == p || num_end > end) {
      continue;
    }
    out = value;
    return true;
  }
  return false;
}

void asciiToLowerInPlace(char* s) {
  if (s == nullptr) {
    return;
  }
  for (; *s != '\0'; ++s) {
    *s = static_cast<char>(std::tolower(static_cast<unsigned char>(*s)));
  }
}

}  // namespace

bool parseGltfMaterialExtrasJson(const char* json, size_t length,
                                 GltfMaterialExtras& out) {
  out = {};
  if (json == nullptr || length == 0) {
    return false;
  }
  const std::string owned(json, length);
  json = owned.c_str();
  const char* end = json + owned.size();
  const char* info_begin = nullptr;
  const char* info_end = nullptr;
  const bool has_info =
      findJsonObjectAfterKey(json, end, "\"material_info\"", info_begin, info_end);

  float metallic = 1.0f;
  float roughness = 1.0f;
  if (has_info) {
    out.has_metallic =
        parseFloatAfterQuotedKey(info_begin, info_end, "\"metallic\"", metallic);
    out.has_roughness =
        parseFloatAfterQuotedKey(info_begin, info_end, "\"roughness\"", roughness);
  }
  if (!out.has_metallic) {
    out.has_metallic =
        parseFloatAfterQuotedKey(json, end, "\"metallic\"", metallic);
  }
  if (!out.has_roughness) {
    out.has_roughness =
        parseFloatAfterQuotedKey(json, end, "\"roughness\"", roughness);
  }
  if (out.has_metallic) {
    out.metallic = metallic;
  }
  if (out.has_roughness) {
    out.roughness = roughness;
  }
  return out.has_metallic || out.has_roughness;
}

bool metallicRoughnessUriIsRoughnessOnly(const char* uri) {
  if (uri == nullptr || uri[0] == '\0') {
    return false;
  }
  char lower[512];
  size_t n = 0;
  while (uri[n] != '\0' && n + 1 < sizeof(lower)) {
    lower[n] = uri[n];
    ++n;
  }
  lower[n] = '\0';
  asciiToLowerInPlace(lower);
  if (std::strstr(lower, "roughness") == nullptr) {
    return false;
  }
  if (std::strstr(lower, "metallic") != nullptr) {
    return false;
  }
  if (std::strstr(lower, "metal_rough") != nullptr) {
    return false;
  }
  if (std::strstr(lower, "orm") != nullptr) {
    return false;
  }
  return true;
}

float resolveImportedMetallicFactor(float gltf_metallic_factor,
                                    const GltfMaterialExtras& extras,
                                    const char* metallic_roughness_uri) {
  if (extras.has_metallic) {
    return extras.metallic;
  }
  if (gltf_metallic_factor > 0.999f &&
      metallicRoughnessUriIsRoughnessOnly(metallic_roughness_uri)) {
    return 0.0f;
  }
  return gltf_metallic_factor;
}

float resolveImportedRoughnessFactor(float gltf_roughness_factor,
                                     const GltfMaterialExtras& extras) {
  if (extras.has_roughness) {
    return extras.roughness;
  }
  return gltf_roughness_factor;
}

}  // namespace Blunder
