#include "runtime/function/editor/animation_preview_controller.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/skeleton.h"
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

AnimationTree* treeFor(Object* object) {
  if (object == nullptr || !object->hasAnimationTree()) {
    return nullptr;
  }
  return object->getAnimationTree();
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

float AnimationPreviewController::timeScale() const {
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr ? player->getTimeScale() : 1.0f;
}

float AnimationPreviewController::blendWeight() const {
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr ? player->getBlendWeight() : 0.0f;
}

const eastl::string& AnimationPreviewController::slotClipName(int slot_index) const {
  static const eastl::string k_empty;
  const AnimationPlayer* player = playerFor(m_target_object);
  if (player == nullptr) {
    return k_empty;
  }
  return player->getSlotClipName(slot_index);
}

void AnimationPreviewController::bindObject(Object* object,
                                            const eastl::string& default_clip_name) {
  m_target_object = nullptr;
  m_default_clip_name.clear();
  m_fade_seconds = 0.0f;
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
  m_fade_seconds = 0.0f;
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
  m_fade_seconds = 0.0f;
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

  const bool started =
      m_fade_seconds > 0.0f
          ? player->play(resolved_name, m_fade_seconds)
          : player->play(resolved_name);
  if (!started) {
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

void AnimationPreviewController::setTimeScale(const float scale) {
  if (AnimationPlayer* player = playerFor(m_target_object)) {
    player->setTimeScale(scale);
  }
}

void AnimationPreviewController::setBlendWeight(const float weight) {
  if (AnimationPlayer* player = playerFor(m_target_object)) {
    player->setBlendWeight(weight);
    resampleBoundSkeleton();
  }
}

void AnimationPreviewController::setFadeSeconds(const float fade_seconds) {
  m_fade_seconds = fade_seconds < 0.0f ? 0.0f : fade_seconds;
}

bool AnimationPreviewController::setSlot(const int slot_index,
                                       const eastl::string& name) {
  AnimationPlayer* player = playerFor(m_target_object);
  if (player == nullptr) {
    return false;
  }
  if (!player->setSlot(slot_index, name)) {
    return false;
  }
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::hasTree() const {
  return treeFor(m_target_object) != nullptr;
}

bool AnimationPreviewController::isTreeActive() const {
  const AnimationTree* tree = treeFor(m_target_object);
  return tree != nullptr && tree->isActive();
}

float AnimationPreviewController::blendSpaceScalar(
    const eastl::string& node_name) const {
  const AnimationTree* tree = treeFor(m_target_object);
  return tree != nullptr ? tree->getBlendSpaceScalar(node_name) : 0.0f;
}

float AnimationPreviewController::add2Weight() const {
  const AnimationTree* tree = treeFor(m_target_object);
  return tree != nullptr ? tree->getAdd2Weight() : 0.0f;
}

bool AnimationPreviewController::setTreeActive(const bool active) {
  AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    return false;
  }
  return tree->setActive(active);
}

bool AnimationPreviewController::travel(const eastl::string& state_name) {
  AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    return false;
  }
  return tree->travel(state_name);
}

bool AnimationPreviewController::start(const eastl::string& state_name) {
  AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    return false;
  }
  return tree->start(state_name);
}

void AnimationPreviewController::setBlendSpaceScalar(
    const eastl::string& node_name, const float scalar) {
  if (AnimationTree* tree = treeFor(m_target_object)) {
    tree->setBlendSpaceScalar(node_name, scalar);
  }
}

bool AnimationPreviewController::requestOneShot(const eastl::string& clip_name) {
  AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    return false;
  }
  return tree->requestOneShot(clip_name);
}

void AnimationPreviewController::setAdd2Weight(const float weight) {
  if (AnimationTree* tree = treeFor(m_target_object)) {
    tree->setAdd2Weight(weight);
  }
}

bool AnimationPreviewController::setAdd2ClipName(const eastl::string& name) {
  AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    return false;
  }
  return tree->setAdd2ClipName(name);
}

void AnimationPreviewController::resampleBoundSkeleton() {
  if (m_target_object == nullptr) {
    return;
  }
  AnimationTree* tree = treeFor(m_target_object);
  if (tree != nullptr && tree->isActive()) {
    tree->sampleBoundSkeleton();
    return;
  }

  AnimationPlayer* player = playerFor(m_target_object);
  Skeleton* skeleton = m_target_object->getSkeleton();
  if (player == nullptr || skeleton == nullptr) {
    return;
  }
  player->sampleOntoSkeleton(*skeleton);
}

void AnimationPreviewController::tick(const float delta_time) {
  if (m_state != AnimationPreviewState::Playing || m_target_object == nullptr) {
    return;
  }
  tickObjectAnimationPreviewFrame(m_target_object, delta_time);
}

}  // namespace Blunder
