#pragma once

#include "EASTL/hash_set.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object.h"
#include "runtime/function/editor/animation_clip_binding_ops.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

struct InspectorAnimationClipRowData final {
  int entry_index{0};
  eastl::string clip_name;
  eastl::string clip_guid;
  eastl::string clip_display;
  bool clip_invalid{false};
};

/// Drop rows whose logical name and GUID are both empty (legacy Add clip drafts).
inline eastl::vector<AnimationPlayer::ClipBinding>
sanitizeClipBindingsDiscardDualEmpty(
    const eastl::vector<AnimationPlayer::ClipBinding>& bindings) {
  eastl::vector<AnimationPlayer::ClipBinding> out;
  out.reserve(bindings.size());
  for (const AnimationPlayer::ClipBinding& binding : bindings) {
    if (binding.name.empty() && binding.guid.empty()) {
      continue;
    }
    out.push_back(binding);
  }
  return out;
}

/// True when every logical name appears at most once (empty names included).
inline bool clipBindingLogicalNamesUnique(
    const eastl::vector<AnimationPlayer::ClipBinding>& bindings) {
  eastl::hash_set<eastl::string> seen;
  for (const AnimationPlayer::ClipBinding& binding : bindings) {
    if (seen.find(binding.name) != seen.end()) {
      return false;
    }
    seen.insert(binding.name);
  }
  return true;
}

inline void buildInspectorAnimationClipRows(
    const Object* object, const AssetRegistry* registry,
    eastl::vector<InspectorAnimationClipRowData>& out_rows) {
  out_rows.clear();
  if (object == nullptr || !object->hasAnimationPlayer()) {
    return;
  }
  const AnimationPlayer* player = object->getAnimationPlayer();
  if (player == nullptr) {
    return;
  }
  const eastl::vector<AnimationPlayer::ClipBinding> bindings =
      player->getClipBindings();
  out_rows.reserve(bindings.size());
  for (int i = 0; i < static_cast<int>(bindings.size()); ++i) {
    InspectorAnimationClipRowData row{};
    row.entry_index = i;
    row.clip_name = bindings[static_cast<size_t>(i)].name;
    row.clip_guid = bindings[static_cast<size_t>(i)].guid;
    row.clip_display =
        animationClipDisplayNameForGuid(row.clip_guid, registry);
    row.clip_invalid =
        row.clip_guid.empty() ||
        (registry != nullptr && registry->resolveGuid(row.clip_guid).empty());
    out_rows.push_back(eastl::move(row));
  }
}

inline eastl::vector<AnimationPlayer::ClipBinding> clipBindingsFromObject(
    const Object* object) {
  if (object == nullptr || !object->hasAnimationPlayer()) {
    return {};
  }
  const AnimationPlayer* player = object->getAnimationPlayer();
  if (player == nullptr) {
    return {};
  }
  return player->getClipBindings();
}

/// Applies bindings after discarding dual-empty rows. Rejects duplicate logical
/// names (no last-write-wins). Returns false when rejected or object missing.
inline bool applyClipBindingsToObject(
    Object* object, const eastl::vector<AnimationPlayer::ClipBinding>& bindings) {
  if (object == nullptr) {
    return false;
  }
  const eastl::vector<AnimationPlayer::ClipBinding> sanitized =
      sanitizeClipBindingsDiscardDualEmpty(bindings);
  if (!clipBindingLogicalNamesUnique(sanitized)) {
    return false;
  }
  AnimationPlayer* player = object->ensureAnimationPlayer();
  if (player == nullptr) {
    return false;
  }
  player->setClipBindings(sanitized);
  return true;
}

}  // namespace Blunder
