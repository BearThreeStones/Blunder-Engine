#include "runtime/function/editor/animation_sync_cine_preview_controller.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/cine_segment_service.h"
#include "runtime/core/object/object.h"
#include "runtime/function/script/animation_frame.h"

namespace Blunder {

bool AnimationSyncCinePreviewController::isInCine() const {
  return cineSegmentService().isInCine();
}

bool AnimationSyncCinePreviewController::isGameplayInputSuppressed() const {
  return cineSegmentService().isGameplayInputSuppressed();
}

void AnimationSyncCinePreviewController::bindObjects(
    eastl::vector<Object*> objects) {
  stop();
  m_objects.clear();

  for (Object* object : objects) {
    if (object != nullptr && object->hasAnimationPlayer()) {
      m_objects.push_back(object);
    }
  }

  rebuildSyncGroup();
}

void AnimationSyncCinePreviewController::clearMembers() {
  stop();
  m_objects.clear();
  rebuildSyncGroup();
}

bool AnimationSyncCinePreviewController::fire(
    const eastl::vector<SyncGroupFireInstruction>& instructions) {
  if (!isValidSyncGroupId(m_sync_group_id) || instructions.empty()) {
    return false;
  }

  const bool fired =
      animationSyncGroupService().fire(m_sync_group_id, instructions);
  if (fired) {
    m_playing = true;
  }
  return fired;
}

bool AnimationSyncCinePreviewController::fireSameName(
    const eastl::string& clip_name) {
  if (!isValidSyncGroupId(m_sync_group_id) || clip_name.empty()) {
    return false;
  }

  const bool fired =
      animationSyncGroupService().fireSameName(m_sync_group_id, clip_name);
  if (fired) {
    m_playing = true;
  }
  return fired;
}

bool AnimationSyncCinePreviewController::enterCine(
    const bool suppress_gameplay_input) {
  return cineSegmentService().enter(suppress_gameplay_input);
}

bool AnimationSyncCinePreviewController::endCine() {
  return cineSegmentService().end();
}

void AnimationSyncCinePreviewController::stop() {
  for (Object* object : m_objects) {
    if (object == nullptr) {
      continue;
    }
    AnimationPlayer* player = object->getAnimationPlayer();
    if (player != nullptr) {
      player->stop();
    }
  }
  m_playing = false;
}

void AnimationSyncCinePreviewController::tick(const float delta_time) {
  if (!m_playing) {
    return;
  }

  for (Object* object : m_objects) {
    tickObjectAnimationPreviewFrame(object, delta_time);
  }
}

void AnimationSyncCinePreviewController::rebuildSyncGroup() {
  AnimationSyncGroupService& service = animationSyncGroupService();
  if (isValidSyncGroupId(m_sync_group_id)) {
    service.destroy(m_sync_group_id);
    m_sync_group_id = k_invalid_sync_group_id;
  }

  if (m_objects.empty()) {
    return;
  }

  m_sync_group_id = service.create();
  for (Object* object : m_objects) {
    AnimationPlayer* player = object->getAnimationPlayer();
    if (player == nullptr) {
      continue;
    }
    service.join(m_sync_group_id, player);
  }
}

}  // namespace Blunder
