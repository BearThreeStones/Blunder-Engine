#include "runtime/function/editor/animation_preview_controller.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/function/editor/inspector_skeleton_modifier_ops.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/function/script/animation_frame.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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

bool AnimationPreviewController::windowBound() const {
  return hasTarget() && hasTree();
}

bool AnimationPreviewController::playEnabled() const {
  if (hasTree()) {
    return true;
  }
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

bool AnimationPreviewController::isLooping() const { return m_session_loop; }

float AnimationPreviewController::playbackPosition() const {
  if (const AnimationTree* tree = treeFor(m_target_object)) {
    return tree->rulerPosition();
  }
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr ? player->getPlaybackPosition() : 0.0f;
}

float AnimationPreviewController::clipLength() const {
  if (const AnimationTree* tree = treeFor(m_target_object)) {
    return tree->rulerLength();
  }
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr ? player->getClipLength() : 0.0f;
}

eastl::string AnimationPreviewController::rulerClipName() const {
  if (const AnimationTree* tree = treeFor(m_target_object)) {
    return tree->rulerClipName();
  }
  const AnimationPlayer* player = playerFor(m_target_object);
  return player != nullptr ? player->getCurrentClipName() : eastl::string();
}

eastl::string AnimationPreviewController::clockReadout() const {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%s  %.2f / %.2f", rulerClipName().c_str(),
                static_cast<double>(playbackPosition()),
                static_cast<double>(clipLength()));
  return eastl::string(buf);
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

void AnimationPreviewController::haltBoundSession() { stop(); }

void AnimationPreviewController::resetSessionChrome() {
  m_default_clip_name.clear();
  m_fade_seconds = 0.0f;
  m_fire_target.clear();
  m_in_cine = false;
  m_input_suppressed = false;
}

void AnimationPreviewController::defaultFireTargetFromBindings() {
  const eastl::vector<eastl::string> names = fireClipNames();
  if (names.empty()) {
    m_fire_target.clear();
    return;
  }
  for (const eastl::string& name : names) {
    if (name == m_fire_target) {
      return;
    }
  }
  m_fire_target = names.front();
}

bool AnimationPreviewController::atRulerEnd() const {
  const float length = clipLength();
  return length > 0.0f && playbackPosition() >= length - 1.0e-3f;
}

void AnimationPreviewController::bindObject(Object* object,
                                            const eastl::string& default_clip_name) {
  if (object == m_target_object) {
    m_default_clip_name = default_clip_name;
    return;
  }

  m_target_object = nullptr;
  resetSessionChrome();
  m_state = AnimationPreviewState::Stopped;

  if (object == nullptr || !object->hasAnimationPlayer()) {
    return;
  }

  m_target_object = object;
  m_default_clip_name = default_clip_name;
  defaultFireTargetFromBindings();
}

void AnimationPreviewController::clearTarget() {
  haltBoundSession();
  m_target_object = nullptr;
  resetSessionChrome();
}

bool AnimationPreviewController::play(const eastl::string& clip_name) {
  if (AnimationTree* tree = treeFor(m_target_object)) {
    if (!tree->isActive()) {
      tree->setActive(true);
    }
    eastl::string resolved_name = clip_name;
    if (resolved_name.empty()) {
      resolved_name = m_default_clip_name;
    }
    if (m_state == AnimationPreviewState::Paused && atRulerEnd()) {
      tree->seekRuler(0.0f);
    }
    if (!resolved_name.empty()) {
      tree->clipPlay(resolved_name);
    }
    m_state = AnimationPreviewState::Playing;
    return true;
  }

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
  if (AnimationTree* tree = treeFor(m_target_object)) {
    tree->clearOneShot();
    tree->clearClipPlay();
    tree->seekRuler(0.0f);
    endCine();
    m_state = AnimationPreviewState::Stopped;
    return;
  }
  if (AnimationPlayer* player = playerFor(m_target_object)) {
    player->stop();
  }
  endCine();
  m_state = AnimationPreviewState::Stopped;
}

void AnimationPreviewController::toggleLoop() { setLoop(!m_session_loop); }

void AnimationPreviewController::setLoop(const bool loop) {
  m_session_loop = loop;
}

void AnimationPreviewController::seekPlayback(const float seconds) {
  if (AnimationTree* tree = treeFor(m_target_object)) {
    tree->seekRuler(seconds);
    return;
  }
  if (AnimationPlayer* player = playerFor(m_target_object)) {
    player->seekPlayback(seconds);
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

bool AnimationPreviewController::blendSpace2DParam(const eastl::string& node_name,
                                                   float& out_x,
                                                   float& out_y) const {
  const AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    out_x = 0.0f;
    out_y = 0.0f;
    return false;
  }
  const BlendSpace2DParam param = tree->getBlendSpace2DParam(node_name);
  out_x = param.x;
  out_y = param.y;
  return true;
}

float AnimationPreviewController::add2Weight() const {
  const AnimationTree* tree = treeFor(m_target_object);
  return tree != nullptr ? tree->getAdd2Weight() : 0.0f;
}

eastl::string AnimationPreviewController::assetGuid() const {
  const AnimationTree* tree = treeFor(m_target_object);
  return tree != nullptr ? tree->getAssetGuid() : eastl::string();
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

void AnimationPreviewController::setBlendSpace2DParam(
    const eastl::string& node_name, const float x, const float y) {
  if (AnimationTree* tree = treeFor(m_target_object)) {
    tree->setBlendSpace2DParam(node_name, x, y);
  }
}

bool AnimationPreviewController::requestOneShot(const eastl::string& clip_name) {
  AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    return false;
  }
  return tree->requestOneShot(clip_name);
}

eastl::vector<eastl::string> AnimationPreviewController::fireClipNames() const {
  eastl::vector<eastl::string> names;
  const AnimationPlayer* player = playerFor(m_target_object);
  if (player == nullptr) {
    return names;
  }
  const eastl::vector<AnimationPlayer::ClipBinding> bindings =
      player->getClipBindings();
  names.reserve(bindings.size());
  for (const AnimationPlayer::ClipBinding& binding : bindings) {
    names.push_back(binding.name);
  }
  return names;
}

void AnimationPreviewController::setFireTarget(const eastl::string& clip_name) {
  m_fire_target = clip_name;
}

bool AnimationPreviewController::fire() {
  if (m_fire_target.empty()) {
    defaultFireTargetFromBindings();
  }
  if (m_fire_target.empty()) {
    return false;
  }
  return requestOneShot(m_fire_target);
}

void AnimationPreviewController::enterCine() {
  m_in_cine = true;
  m_input_suppressed = true;
}

void AnimationPreviewController::endCine() {
  m_in_cine = false;
  m_input_suppressed = false;
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

void AnimationPreviewController::setAssetGuid(const eastl::string& guid) {
  if (AnimationTree* tree = treeFor(m_target_object)) {
    tree->setAssetGuid(guid);
  }
}

bool AnimationPreviewController::applyTreeTopology(
    const AnimationTreeTopologyData& topology) {
  AnimationTree* tree = treeFor(m_target_object);
  if (tree == nullptr) {
    return false;
  }
  return tree->applyTopologyData(topology);
}

void AnimationPreviewController::applyTreeOverrides(
    const AnimationTreeInstanceOverrides& overrides) {
  if (AnimationTree* tree = treeFor(m_target_object)) {
    tree->applyInstanceOverrides(overrides);
  }
}

size_t AnimationPreviewController::skeletonModifierCount() const {
  return m_target_object != nullptr ? m_target_object->getSkeletonModifierCount()
                                    : 0;
}

bool AnimationPreviewController::addSkeletonModifier(
    const eastl::string& type_name) {
  if (m_target_object == nullptr) {
    return false;
  }
  if (addSkeletonModifierByType(m_target_object, type_name) == nullptr) {
    return false;
  }
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::removeSkeletonModifier(const size_t index) {
  if (m_target_object == nullptr) {
    return false;
  }
  if (!m_target_object->removeSkeletonModifierAt(index)) {
    return false;
  }
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::setSkeletonModifierEnabled(const size_t index,
                                                            const bool enabled) {
  if (m_target_object == nullptr) {
    return false;
  }
  SkeletonModifier* modifier = m_target_object->getSkeletonModifierAt(index);
  if (modifier == nullptr) {
    return false;
  }
  modifier->setEnabled(enabled);
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::isSkeletonModifierEnabled(
    const size_t index) const {
  if (m_target_object == nullptr) {
    return false;
  }
  const SkeletonModifier* modifier =
      m_target_object->getSkeletonModifierAt(index);
  return modifier != nullptr && modifier->isEnabled();
}

bool AnimationPreviewController::moveSkeletonModifier(const size_t from_index,
                                                      const size_t to_index) {
  if (m_target_object == nullptr) {
    return false;
  }
  if (!m_target_object->moveSkeletonModifier(from_index, to_index)) {
    return false;
  }
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::setSkeletonLookAtTarget(
    const size_t modifier_index, const Vec3& target) {
  if (m_target_object == nullptr) {
    return false;
  }
  SkeletonModifier* modifier =
      m_target_object->getSkeletonModifierAt(modifier_index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "SkeletonLookAtModifier") != 0) {
    return false;
  }
  auto* look_at = static_cast<SkeletonLookAtModifier*>(modifier);
  look_at->setTarget(target);
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::setSkeletonLookAtBoneName(
    const size_t modifier_index, const eastl::string& bone_name) {
  if (m_target_object == nullptr) {
    return false;
  }
  SkeletonModifier* modifier =
      m_target_object->getSkeletonModifierAt(modifier_index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "SkeletonLookAtModifier") != 0) {
    return false;
  }
  auto* look_at = static_cast<SkeletonLookAtModifier*>(modifier);
  look_at->setBoneName(bone_name);
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::setSkeletonPaperMouthOpenAmount(
    const size_t modifier_index, const float open_amount) {
  if (m_target_object == nullptr) {
    return false;
  }
  SkeletonModifier* modifier =
      m_target_object->getSkeletonModifierAt(modifier_index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "PaperMouth") != 0) {
    return false;
  }
  auto* paper_mouth = static_cast<SkeletonPaperMouthModifier*>(modifier);
  paper_mouth->setOpenAmount(open_amount);
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::setSkeletonPaperMouthBoneName(
    const size_t modifier_index, const eastl::string& bone_name) {
  if (m_target_object == nullptr) {
    return false;
  }
  SkeletonModifier* modifier =
      m_target_object->getSkeletonModifierAt(modifier_index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "PaperMouth") != 0) {
    return false;
  }
  auto* paper_mouth = static_cast<SkeletonPaperMouthModifier*>(modifier);
  paper_mouth->setBoneName(bone_name);
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::setSkeletonAttachBoneName(
    const size_t modifier_index, const eastl::string& bone_name) {
  if (m_target_object == nullptr) {
    return false;
  }
  SkeletonModifier* modifier =
      m_target_object->getSkeletonModifierAt(modifier_index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "SkeletonAttachModifier") != 0) {
    return false;
  }
  auto* attach = static_cast<SkeletonAttachModifier*>(modifier);
  attach->setBoneName(bone_name);
  resampleBoundSkeleton();
  return true;
}

bool AnimationPreviewController::setSkeletonAttachChildObjectId(
    const size_t modifier_index, const ObjectId child_object_id) {
  if (m_target_object == nullptr) {
    return false;
  }
  SkeletonModifier* modifier =
      m_target_object->getSkeletonModifierAt(modifier_index);
  if (modifier == nullptr ||
      std::strcmp(modifier->getTypeName(), "SkeletonAttachModifier") != 0) {
    return false;
  }
  auto* attach = static_cast<SkeletonAttachModifier*>(modifier);
  attach->setChildObjectId(child_object_id);
  resampleBoundSkeleton();
  return true;
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

  AnimationTree* tree = treeFor(m_target_object);
  if (tree != nullptr) {
    const float length = clipLength();
    const float position = playbackPosition();
    const float scale = timeScale();
    const eastl::string oneshot_name = tree->getOneShotClipName();
    const bool oneshot_before = tree->isOneShotActive();
    if (length > 0.0f) {
      if (!m_session_loop &&
          position + delta_time * scale >= length - 1.0e-6f) {
        seekPlayback(length);
        pause();
        return;
      }
    }
    tickObjectAnimationPreviewFrame(m_target_object, delta_time);
    if (m_session_loop) {
      if (oneshot_before && !tree->isOneShotActive() &&
          !oneshot_name.empty()) {
        tree->requestOneShot(oneshot_name);
      } else {
        const float wrapped_length = clipLength();
        const float wrapped_position = playbackPosition();
        if (wrapped_length > 0.0f && wrapped_position >= wrapped_length) {
          const float wrapped = std::fmod(wrapped_position, wrapped_length);
          seekPlayback(wrapped < 0.0f ? wrapped + wrapped_length : wrapped);
        }
      }
    }
    return;
  }

  tickObjectAnimationPreviewFrame(m_target_object, delta_time);
}

}  // namespace Blunder
