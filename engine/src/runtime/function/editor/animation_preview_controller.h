#pragma once

#include "EASTL/string.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class Object;
class SceneInstance;

enum class AnimationPreviewState : uint8_t {
  Stopped = 0,
  Playing,
  Paused,
};

/// Edit Mode AnimationPlayer preview — no DotNetHost / Behaviour Tick.
class AnimationPreviewController final {
 public:
  bool hasTarget() const { return m_target_object != nullptr; }
  AnimationPreviewState state() const { return m_state; }

  bool playEnabled() const;
  bool pauseEnabled() const;
  bool stopEnabled() const;
  bool isLooping() const;
  bool isPaused() const { return m_state == AnimationPreviewState::Paused; }

  const eastl::string& defaultClipName() const { return m_default_clip_name; }
  float playbackPosition() const;
  float clipLength() const;

  void bindObject(Object* object, const eastl::string& default_clip_name = {});
  void bindSelection(SceneInstance* scene, EntityId entity_id);
  void clearTarget();

  bool play(const eastl::string& clip_name = {});
  bool pause();
  bool resume();
  void stop();
  void toggleLoop();
  void setLoop(bool loop);

  /// Advance preview playback via tickObjectAnimationPreviewFrame when playing.
  void tick(float delta_time);

 private:
  Object* m_target_object{nullptr};
  eastl::string m_default_clip_name;
  AnimationPreviewState m_state{AnimationPreviewState::Stopped};
};

}  // namespace Blunder
