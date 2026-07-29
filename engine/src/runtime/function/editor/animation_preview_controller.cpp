#include "runtime/function/editor/animation_preview_controller.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object.h"
#include "runtime/function/editor/animation_clip_resolve.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/script/animation_frame.h"

namespace Blunder {

namespace {

AnimationPlayer* playerFor(Object* object) {
  if (object == nullptr || !object->hasAnimationPlayer()) {
    return nullptr;
  }
  return object->getAnimationPlayer();
}

}  // namespace

bool AnimationPreviewController::playEnabled() const {
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr && player->getClipMapEntryCount() > 0;
}

bool AnimationPreviewController::pauseEnabled() const {
  return m_state == AnimationPreviewState::Playing ||
         m_state == AnimationPreviewState::Paused;
}

bool AnimationPreviewController::stopEnabled() const {
  return m_state != AnimationPreviewState::Stopped;
}

bool AnimationPreviewController::isLooping() const {
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr && player->isLooping();
}

float AnimationPreviewController::playbackPosition() const {
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr ? player->getPlaybackPosition() : 0.0f;
}

float AnimationPreviewController::clipLength() const {
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr ? player->getClipLength() : 0.0f;
}

void AnimationPreviewController::bindObject(Object* object,
                                            const eastl::string& default_clip_name) {
  m_target_object = nullptr;
  m_default_clip_name.clear();
  m_state = AnimationPreviewState::Stopped;

  if (object == nullptr || !object->hasAnimationPlayer()) {
    return;
  }

  wireAnimationPlayerAssetResolver(*object->getAnimationPlayer());
  m_target_object = object;
  m_default_clip_name = default_clip_name;
}

void AnimationPreviewController::bindSelection(SceneInstance* scene,
                                             EntityId entity_id) {
  m_target_object = nullptr;
  m_default_clip_name.clear();
  m_state = AnimationPreviewState::Stopped;

  if (scene == nullptr || !isValid(entity_id)) {
    return;
  }

  Object* object = scene->findBoundObject(entity_id);
  if (object == nullptr) {
    object = scene->ensureBoundObject(entity_id);
  }
  if (object == nullptr || !object->hasAnimationPlayer()) {
    return;
  }

  bindObject(object, scene->getDefaultAnimationClipName(entity_id));
}

void AnimationPreviewController::clearTarget() {
  stop();
  m_target_object = nullptr;
  m_default_clip_name.clear();
}

bool AnimationPreviewController::play(const eastl::string& clip_name) {
  AnimationPlayer* player = playerFor(m_target_object);
  if (player == nullptr) {
    return false;
  }

  eastl::string resolved_name = clip_name;
  if (resolved_name.empty()) {
    if (!m_default_clip_name.empty()) {
      resolved_name = m_default_clip_name;
    } else if (!player->getCurrentClipName().empty()) {
      resolved_name = player->getCurrentClipName();
    } else {
      return false;
    }
  }

  if (!player->play(resolved_name)) {
    return false;
  }

  m_default_clip_name = resolved_name;
  m_state = AnimationPreviewState::Playing;
  return true;
}

bool AnimationPreviewController::pause() {
  if (m_state != AnimationPreviewState::Playing) {
    return false;
  }
  m_state = AnimationPreviewState::Paused;
  return true;
}

bool AnimationPreviewController::resume() {
  if (m_state != AnimationPreviewState::Paused) {
    return false;
  }
  m_state = AnimationPreviewState::Playing;
  return true;
}

void AnimationPreviewController::stop() {
  if (AnimationPlayer* player = playerFor(m_target_object)) {
    player->stop();
  }
  m_state = AnimationPreviewState::Stopped;
}

void AnimationPreviewController::toggleLoop() {
  if (AnimationPlayer* player = playerFor(m_target_object)) {
    player->setLoop(!player->isLooping());
  }
}

void AnimationPreviewController::setLoop(const bool loop) {
  if (AnimationPlayer* player = playerFor(m_target_object)) {
    player->setLoop(loop);
  }
}

void AnimationPreviewController::tick(const float delta_time) {
  if (m_state != AnimationPreviewState::Playing || m_target_object == nullptr) {
    return;
  }
  tickObjectAnimationPreviewFrame(m_target_object, delta_time);
}

}  // namespace Blunder
