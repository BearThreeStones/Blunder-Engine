#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object.h"

namespace Blunder {

struct InspectorAnimationClipRowData final {
  int entry_index{0};
  eastl::string clip_name;
  eastl::string clip_guid;
};

inline void buildInspectorAnimationClipRows(
    const Object* object, eastl::vector<InspectorAnimationClipRowData>& out_rows) {
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

inline void applyClipBindingsToObject(
    Object* object, const eastl::vector<AnimationPlayer::ClipBinding>& bindings) {
  if (object == nullptr) {
    return;
  }
  AnimationPlayer* player = object->ensureAnimationPlayer();
  if (player == nullptr) {
    return;
  }
  player->setClipBindings(bindings);
}

}  // namespace Blunder
