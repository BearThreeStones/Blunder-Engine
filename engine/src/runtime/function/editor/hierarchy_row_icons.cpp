#include "runtime/function/editor/hierarchy_row_icons.h"

#include "runtime/core/object/object.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

void fillHierarchyRowIcons(const SceneInstance& scene, EntityId entity_id,
                           eastl::vector<HierarchyRowIconSlot>& out) {
  out.clear();
  if (!isValid(entity_id) || scene.getEntity(entity_id) == nullptr) {
    return;
  }

  HierarchyRowIconSlot transform{};
  transform.kind = HierarchyRowIconKind::Transform;
  transform.index = 0;
  out.push_back(transform);

  if (scene.getMeshRenderer(entity_id) != nullptr) {
    HierarchyRowIconSlot slot{};
    slot.kind = HierarchyRowIconKind::MeshRenderer;
    out.push_back(slot);
  }
  if (scene.getCamera(entity_id) != nullptr) {
    HierarchyRowIconSlot slot{};
    slot.kind = HierarchyRowIconKind::Camera;
    out.push_back(slot);
  }
  if (scene.getLight(entity_id) != nullptr) {
    HierarchyRowIconSlot slot{};
    slot.kind = HierarchyRowIconKind::Light;
    out.push_back(slot);
  }

  const Object* object = scene.findBoundObject(entity_id);
  if (object == nullptr) {
    return;
  }

  if (object->hasSkeleton()) {
    HierarchyRowIconSlot slot{};
    slot.kind = HierarchyRowIconKind::Skeleton;
    out.push_back(slot);
  }
  if (object->hasAnimationTree()) {
    HierarchyRowIconSlot slot{};
    slot.kind = HierarchyRowIconKind::AnimationTree;
    out.push_back(slot);
  }

  const size_t behaviour_count = object->getBehaviourCount();
  for (size_t i = 0; i < behaviour_count; ++i) {
    HierarchyRowIconSlot slot{};
    slot.kind = HierarchyRowIconKind::Behaviour;
    slot.index = static_cast<int32_t>(i);
    out.push_back(slot);
  }

  const size_t modifier_count = object->getSkeletonModifierCount();
  for (size_t i = 0; i < modifier_count; ++i) {
    HierarchyRowIconSlot slot{};
    slot.kind = HierarchyRowIconKind::SkeletonModifier;
    slot.index = static_cast<int32_t>(i);
    out.push_back(slot);
  }
}

float hierarchyRowIconStripWidth(size_t icon_count) {
  if (icon_count == 0) {
    return 0.0f;
  }
  return static_cast<float>(icon_count) * k_hierarchy_row_icon_px +
         static_cast<float>(icon_count - 1) * k_hierarchy_row_icon_gap_px;
}

int hitTestHierarchyRowIconIndex(float mouse_x, float row_width,
                                 size_t icon_count) {
  if (icon_count == 0 || row_width <= 0.0f) {
    return -1;
  }
  const float strip_w = hierarchyRowIconStripWidth(icon_count);
  const float strip_left = row_width - strip_w;
  if (mouse_x < strip_left || mouse_x >= row_width) {
    return -1;
  }
  const float stride = k_hierarchy_row_icon_px + k_hierarchy_row_icon_gap_px;
  int index = static_cast<int>((mouse_x - strip_left) / stride);
  if (index < 0) {
    return -1;
  }
  if (static_cast<size_t>(index) >= icon_count) {
    index = static_cast<int>(icon_count - 1);
  }
  return index;
}

bool hierarchyRowIconAttachmentPresent(const SceneInstance& scene,
                                       EntityId entity_id,
                                       HierarchyRowIconKind kind,
                                       int32_t index) {
  const auto* entity = scene.getEntity(entity_id);
  if (entity == nullptr || entity->isTombstoned()) {
    return false;
  }

  switch (kind) {
    case HierarchyRowIconKind::Transform:
      return true;
    case HierarchyRowIconKind::MeshRenderer:
      return scene.getMeshRenderer(entity_id) != nullptr;
    case HierarchyRowIconKind::Camera:
      return scene.getCamera(entity_id) != nullptr;
    case HierarchyRowIconKind::Light:
      return scene.getLight(entity_id) != nullptr;
    case HierarchyRowIconKind::Skeleton: {
      const Object* object = scene.findBoundObject(entity_id);
      return object != nullptr && object->hasSkeleton();
    }
    case HierarchyRowIconKind::AnimationTree: {
      const Object* object = scene.findBoundObject(entity_id);
      return object != nullptr && object->hasAnimationTree();
    }
    case HierarchyRowIconKind::Behaviour: {
      const Object* object = scene.findBoundObject(entity_id);
      if (object == nullptr || index < 0) {
        return false;
      }
      return static_cast<size_t>(index) < object->getBehaviourCount();
    }
    case HierarchyRowIconKind::SkeletonModifier: {
      const Object* object = scene.findBoundObject(entity_id);
      if (object == nullptr || index < 0) {
        return false;
      }
      return static_cast<size_t>(index) < object->getSkeletonModifierCount();
    }
  }
  return false;
}

}  // namespace Blunder
