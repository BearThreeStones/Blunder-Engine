#include "runtime/function/editor/animation_preview_controller.h"

#include "runtime/core/object/object.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

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

}  // namespace Blunder
