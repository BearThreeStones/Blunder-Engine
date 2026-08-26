#include "runtime/function/editor/animation_preview_controller.h"

#include "runtime/core/object/object.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

void AnimationPreviewController::bindSelection(SceneInstance* scene,
                                             EntityId entity_id) {
  bindSelection(scene, entity_id, isValid(entity_id) ? 1 : 0);
}

void AnimationPreviewController::bindSelection(SceneInstance* scene,
                                             EntityId entity_id,
                                             size_t selected_count) {
  if (scene == nullptr || selected_count != 1 || !isValid(entity_id)) {
    clearTarget();
    return;
  }

  Object* object = scene->findBoundObject(entity_id);
  if (object == nullptr) {
    object = scene->ensureBoundObject(entity_id);
  }
  if (object == nullptr || !object->hasAnimationTree()) {
    clearTarget();
    return;
  }

  if (object != m_target_object && m_target_object != nullptr) {
    haltBoundSession();
  }
  bindObject(object, scene->getDefaultAnimationClipName(entity_id));
}

}  // namespace Blunder
