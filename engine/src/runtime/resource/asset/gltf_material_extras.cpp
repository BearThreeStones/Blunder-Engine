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

bool parseVec3ArrayAfterQuotedKey(const char* begin, const char* end,
                                  const char* key, float out[3]) {
  if (begin == nullptr || end == nullptr || key == nullptr || out == nullptr ||
      begin >= end) {
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
    if (p >= end || *p != '[') {
      continue;
    }
    ++p;
    bool ok = true;
    for (int i = 0; i < 3; ++i) {
      p = skipWs(p, end);
      if (p >= end) {
        ok = false;
        break;
      }
      char* num_end = nullptr;
      const float value = std::strtof(p, &num_end);
      if (num_end == p || num_end > end) {
        ok = false;
        break;
      }
      out[i] = value;
      p = num_end;
      p = skipWs(p, end);
      if (i < 2) {
        if (p >= end || *p != ',') {
          ok = false;
          break;
        }
        ++p;
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
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

  float paper[3] = {1.0f, 1.0f, 1.0f};
  if (has_info) {
    out.has_paper_color =
        parseVec3ArrayAfterQuotedKey(info_begin, info_end, "\"paper_color\"",
                                     paper);
  }
  if (!out.has_paper_color) {
    out.has_paper_color =
        parseVec3ArrayAfterQuotedKey(json, end, "\"paper_color\"", paper);
  }
  if (out.has_paper_color) {
    out.paper_color[0] = paper[0];
    out.paper_color[1] = paper[1];
    out.paper_color[2] = paper[2];
  }
  return out.has_metallic || out.has_roughness || out.has_paper_color;
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

bool textureUriIsPaperGrain(const char* uri) {
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
  if (std::strstr(lower, "paper_rough") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "paper-rough") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "paper_grain") != nullptr) {
    return true;
  }
  return std::strstr(lower, "papergrain") != nullptr;
}

bool materialNameIsDummyPath(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  return std::strncmp(name, "DUMMY-path", 10) == 0;
}

bool textureUriIsPathPaperAlbedo(const char* uri) {
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
  if (std::strstr(lower, "path_1_albedo") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "path_2_albedo") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "path_7_albedo") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "path_faint_albedo") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "path_snowman") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "path_2_connection") != nullptr) {
    return true;
  }
  if (std::strstr(lower, "path-albedo") != nullptr) {
    return true;
  }
  return std::strstr(lower, "path_albedo") != nullptr;
}

bool meshSourceLooksLikeDummyPathSet(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  if (std::strstr(path, "SL-fence-paths") != nullptr) {
    return true;
  }
  if (std::strstr(path, "SL-hub-paths") != nullptr) {
    return true;
  }
  return std::strstr(path, "SL-clearing-path") != nullptr;
}

const char* dummyPathAlbedoFileName(const char* material_name) {
  if (!materialNameIsDummyPath(material_name)) {
    return nullptr;
  }
  const char* rest = material_name + 10;
  if (*rest == '-') {
    ++rest;
  }
  if (std::strcmp(rest, "pond") == 0) {
    return "path_7_albedo.png";
  }
  if (std::strcmp(rest, "fence") == 0) {
    return "path_1_albedo.png";
  }
  if (std::strcmp(rest, "hub") == 0) {
    return "path_2_albedo.png";
  }
  if (std::strcmp(rest, "faint") == 0) {
    return "path_faint_albedo.png";
  }
  if (std::strcmp(rest, "snowman") == 0) {
    return "path_snowman_1.png";
  }
  if (std::strcmp(rest, "hub_connection") == 0) {
    return "path_2_connection.png";
  }
  return nullptr;
}

void dummyPathFallbackAlbedoRgb(float out[3]) {
  if (out == nullptr) {
    return;
  }
  // Godot path_1_albedo mid-dirt (sRGB approx).
  out[0] = 0.55f;
  out[1] = 0.38f;
  out[2] = 0.28f;
}

bool materialNameIsDummySnowPatch(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  return std::strncmp(name, "DUMMY-snow_patch", 16) == 0;
}

bool textureUriIsSnowPatchAlbedo(const char* uri) {
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
  if (std::strstr(lower, "snow_gen_albedo") != nullptr) {
    return true;
  }
  return std::strstr(lower, "snow-gen-albedo") != nullptr;
}

bool meshSourceLooksLikeDummySnowPatchSet(const char* path) {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  if (std::strstr(path, "SL-hub-snow_patches") != nullptr) {
    return true;
  }
  return std::strstr(path, "SL-fence-snow_patches") != nullptr;
}

const char* dummySnowPatchAlbedoFileName(const char* material_name) {
  if (!materialNameIsDummySnowPatch(material_name)) {
    return nullptr;
  }
  return "snow_gen_albedo-01.png";
}

void dummySnowPatchFallbackAlbedoRgb(float out[3]) {
  if (out == nullptr) {
    return;
  }
  // Godot snow_patch_01.tres albedo_color (0.92, 0.971, 1).
  out[0] = 0.92f;
  out[1] = 0.971f;
  out[2] = 1.0f;
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
