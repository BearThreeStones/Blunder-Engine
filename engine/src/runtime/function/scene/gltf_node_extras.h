#pragma once

#include <cgltf.h>

#include <cstring>

#include "EASTL/string.h"

namespace Blunder {

inline bool gltfNodeNameStartsWith(const cgltf_node* node, const char* prefix) {
  if (node == nullptr || prefix == nullptr || node->name == nullptr) {
    return false;
  }
  const size_t n = std::strlen(prefix);
  return n > 0 && std::strncmp(node->name, prefix, n) == 0;
}

/// Reads Godot blender-studio `instance_asset_id` from node extras JSON.
inline bool gltfNodeInstanceAssetId(const cgltf_node* node, eastl::string& out_id) {
  out_id.clear();
  if (node == nullptr || node->extras.data == nullptr || node->extras.data[0] == '\0') {
    return false;
  }
  const char* json = node->extras.data;
  const char* key = std::strstr(json, "\"instance_asset_id\"");
  if (key == nullptr) {
    return false;
  }
  const char* colon = std::strchr(key, ':');
  if (colon == nullptr) {
    return false;
  }
  const char* quote = std::strchr(colon, '"');
  if (quote == nullptr) {
    return false;
  }
  const char* end = std::strchr(quote + 1, '"');
  if (end == nullptr || end <= quote + 1) {
    return false;
  }
  out_id.assign(quote + 1, static_cast<eastl::string::size_type>(end - quote - 1));
  return !out_id.empty();
}

}  // namespace Blunder
