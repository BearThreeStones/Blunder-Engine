#include "runtime/core/object/animation_method_dispatch.h"

#include <algorithm>
#include <cmath>

#include "runtime/core/reflection/message_dispatch.h"

namespace Blunder {

namespace {

void dispatchMethodKeysInRange(ObjectId target,
                             const eastl::vector<AnimationMethodKey>& keys,
                             float range_start, float range_end) {
  if (!isValid(target) || keys.empty()) {
    return;
  }

  for (const AnimationMethodKey& key : keys) {
    if (key.name.empty()) {
      continue;
    }
    if (key.time <= range_start || key.time > range_end) {
      continue;
    }

    const MessageId message_id =
        MessageDispatch::registerName(key.name.c_str());
    if (message_id == k_invalid_message_id) {
      continue;
    }

    const int argc =
        static_cast<int>(std::min(key.args.size(), static_cast<size_t>(4)));
    MessageArg args[4]{};
    for (int index = 0; index < argc; ++index) {
      args[index].kind = MessageArgKind::Float;
      args[index].f = key.args[static_cast<size_t>(index)];
    }
  MessageDispatch::send(target, message_id, argc > 0 ? args : nullptr, argc);
  }
}

}  // namespace

void dispatchAnimationMethodKeysCrossed(ObjectId target,
                                        const AnimationClipData& clip,
                                        float prev_time, float new_time,
                                        bool looping) {
  if (!isValid(target) || clip.method_keys.empty()) {
    return;
  }

  if (new_time < prev_time) {
    return;
  }

  const float duration = clip.duration;
  if (!looping || duration <= 0.0f || new_time <= duration) {
    dispatchMethodKeysInRange(target, clip.method_keys, prev_time, new_time);
    return;
  }

  if (prev_time < duration) {
    dispatchMethodKeysInRange(target, clip.method_keys, prev_time, duration);
  }
  dispatchMethodKeysInRange(target, clip.method_keys, 0.0f,
                            std::fmod(new_time, duration));
}

}  // namespace Blunder
