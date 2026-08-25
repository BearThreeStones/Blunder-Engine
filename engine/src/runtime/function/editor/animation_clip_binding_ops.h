#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

struct AnimationClipPickerEntry final {
  eastl::string guid;
  eastl::string display_name;
  eastl::string stem;
};

inline bool endsWithAnimationClipDescriptor(const eastl::string& virtual_path) {
  const char* suffix = ".animation.yaml";
  const size_t suffix_len = 15;
  return virtual_path.size() >= suffix_len &&
         virtual_path.compare(virtual_path.size() - suffix_len, suffix_len,
                              suffix) == 0;
}

inline eastl::string basenameFromVirtualPath(const eastl::string& virtual_path) {
  if (virtual_path.empty()) {
    return {};
  }
  size_t end = virtual_path.size();
  if (virtual_path.back() == '/') {
    end -= 1;
  }
  const size_t slash = virtual_path.find_last_of('/', end - 1);
  if (slash == eastl::string::npos) {
    return virtual_path.substr(0, end);
  }
  return virtual_path.substr(slash + 1, end - slash - 1);
}

/// Descriptor basename without `.animation.yaml` — default logical clip name.
inline eastl::string animationClipStemFromDescriptorPath(
    const eastl::string& virtual_path) {
  eastl::string base = basenameFromVirtualPath(virtual_path);
  const char* suffix = ".animation.yaml";
  const size_t suffix_len = 15;
  if (base.size() >= suffix_len &&
      base.compare(base.size() - suffix_len, suffix_len, suffix) == 0) {
    base.resize(base.size() - suffix_len);
  }
  return base;
}

inline eastl::string animationClipDisplayNameForGuid(
    const eastl::string& guid, const AssetRegistry* registry) {
  if (guid.empty()) {
    return eastl::string("(missing clip)");
  }
  if (registry == nullptr) {
    return guid;
  }
  const eastl::string path = registry->resolveGuid(guid);
  if (path.empty()) {
    return eastl::string("(missing clip)");
  }
  return basenameFromVirtualPath(path);
}

inline void listRegisteredAnimationClips(
    const AssetRegistry* registry,
    eastl::vector<AnimationClipPickerEntry>& out_entries) {
  out_entries.clear();
  if (registry == nullptr) {
    return;
  }
  const eastl::vector<eastl::pair<eastl::string, eastl::string>> entries =
      registry->registeredEntries();
  out_entries.reserve(entries.size());
  for (const eastl::pair<eastl::string, eastl::string>& entry : entries) {
    if (!endsWithAnimationClipDescriptor(entry.second)) {
      continue;
    }
    AnimationClipPickerEntry row{};
    row.guid = entry.first;
    row.display_name = basenameFromVirtualPath(entry.second);
    row.stem = animationClipStemFromDescriptorPath(entry.second);
    out_entries.push_back(eastl::move(row));
  }
}

/// Content Browser AnimationClip drop hit: >=0 retarget row; -1 append; -2 miss.
inline constexpr int k_animation_clip_drop_append = -1;
inline constexpr int k_animation_clip_drop_miss = -2;

/// Hit-test Inspector clip list geometry (logical window coords). Row pitch must
/// match Slint fixed clip-row height. Add-clip band is append; outside is miss.
inline int hitTestAnimationClipDropTarget(
    float logical_x, float logical_y, float list_x, float list_y, float list_w,
    float row_pitch, int clip_count, float add_clip_y, float add_clip_h) {
  if (list_w <= 0.0f || row_pitch <= 0.0f) {
    return k_animation_clip_drop_miss;
  }
  if (logical_x < list_x || logical_x >= list_x + list_w) {
    return k_animation_clip_drop_miss;
  }
  if (clip_count < 0) {
    clip_count = 0;
  }
  if (clip_count > 0) {
    const float rows_bottom = list_y + static_cast<float>(clip_count) * row_pitch;
    if (logical_y >= list_y && logical_y < rows_bottom) {
      const int index =
          static_cast<int>((logical_y - list_y) / row_pitch);
      if (index >= 0 && index < clip_count) {
        return index;
      }
    }
  }
  if (add_clip_h > 0.0f && logical_y >= add_clip_y &&
      logical_y < add_clip_y + add_clip_h) {
    return k_animation_clip_drop_append;
  }
  // Empty list: treat the list host band down to Add clip as append chrome.
  if (clip_count == 0 && logical_y >= list_y &&
      logical_y < add_clip_y + add_clip_h) {
    return k_animation_clip_drop_append;
  }
  return k_animation_clip_drop_miss;
}

}  // namespace Blunder
