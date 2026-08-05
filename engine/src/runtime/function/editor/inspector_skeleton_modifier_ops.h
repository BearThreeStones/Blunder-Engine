#pragma once

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

struct InspectorSkeletonModifierRowData final {
  size_t index{0};
  eastl::string type_name;
  bool enabled{true};
  eastl::string bone_name;
  float open_amount{0.0f};
  bool attach_driven{false};
  Vec3 target{0.0f, 0.0f, 1.0f};
  eastl::string child_entity_name;
};

inline SkeletonModifier* addSkeletonModifierByType(Object* object,
                                                 const eastl::string& type) {
  if (object == nullptr) {
    return nullptr;
  }
  if (type == "PaperMouth") {
    return object->addSkeletonPaperMouthModifier();
  }
  if (type == "SkeletonAttachModifier") {
    return object->addSkeletonAttachModifier();
  }
  if (type == "SkeletonLookAtModifier") {
    return object->addSkeletonLookAtModifier();
  }
  if (type == "SkeletonModifier") {
    return object->addSkeletonModifier();
  }
  return nullptr;
}

inline eastl::unique_ptr<SkeletonModifier> makeSkeletonModifierFromDef(
    const SceneSkeletonModifierDef& def) {
  if (def.type == "PaperMouth") {
    auto mouth = eastl::make_unique<SkeletonPaperMouthModifier>();
    if (!def.bone_name.empty()) {
      mouth->setBoneName(def.bone_name);
    }
    mouth->setAttachDriven(def.attach_driven);
    mouth->setOpenAmount(def.open_amount);
    mouth->setEnabled(def.enabled);
    return mouth;
  }
  if (def.type == "SkeletonLookAtModifier") {
    auto look_at = eastl::make_unique<SkeletonLookAtModifier>();
    if (!def.bone_name.empty()) {
      look_at->setBoneName(def.bone_name);
    }
    look_at->setTarget(def.target);
    look_at->setEnabled(def.enabled);
    return look_at;
  }
  if (def.type == "SkeletonAttachModifier") {
    auto attach = eastl::make_unique<SkeletonAttachModifier>();
    if (!def.bone_name.empty()) {
      attach->setBoneName(def.bone_name);
    }
    attach->setEnabled(def.enabled);
    return attach;
  }
  if (def.type == "SkeletonModifier") {
    auto slot = eastl::make_unique<SkeletonModifier>();
    slot->setEnabled(def.enabled);
    return slot;
  }
  return nullptr;
}

inline void wireAttachChildFromScene(SceneInstance* scene, Object* host,
                                     size_t modifier_index,
                                     const eastl::string& child_entity_name) {
  if (scene == nullptr || host == nullptr || child_entity_name.empty()) {
    return;
  }
  SkeletonModifier* slot = host->getSkeletonModifierAt(modifier_index);
  if (slot == nullptr ||
      eastl::string(slot->getTypeName()) != "SkeletonAttachModifier") {
    return;
  }
  const EntityId child_id = scene->findEntityByName(child_entity_name);
  if (!isValid(child_id)) {
    return;
  }
  Object* child = scene->ensureBoundObject(child_id);
  if (child == nullptr) {
    return;
  }
  static_cast<SkeletonAttachModifier*>(slot)->setChildObjectId(child->getId());
}

inline bool captureSkeletonModifierDef(const SceneInstance& scene,
                                       const Object& object, size_t index,
                                       SceneSkeletonModifierDef& out_def) {
  const SkeletonModifier* modifier = object.getSkeletonModifierAt(index);
  if (modifier == nullptr) {
    return false;
  }
  out_def = SceneSkeletonModifierDef{};
  out_def.type = modifier->getTypeName();
  out_def.enabled = modifier->isEnabled();

  if (out_def.type == "PaperMouth") {
    const auto* mouth = static_cast<const SkeletonPaperMouthModifier*>(modifier);
    out_def.bone_name = mouth->getBoneName();
    out_def.open_amount = mouth->getOpenAmount();
    out_def.attach_driven = mouth->isAttachDriven();
  } else if (out_def.type == "SkeletonLookAtModifier") {
    const auto* look_at =
        static_cast<const SkeletonLookAtModifier*>(modifier);
    out_def.bone_name = look_at->getBoneName();
    out_def.target = look_at->getTarget();
  } else if (out_def.type == "SkeletonAttachModifier") {
    const auto* attach = static_cast<const SkeletonAttachModifier*>(modifier);
    out_def.bone_name = attach->getBoneName();
    if (const Object* child = ObjectDB::get(attach->getChildObjectId())) {
      const Entity* child_entity = scene.getEntity(child->getEntityId());
      out_def.child_entity_name = child_entity != nullptr
                                      ? child_entity->getName()
                                      : child->getName();
    }
  }
  return true;
}

inline void applySkeletonModifierFieldsOnObject(SceneInstance* scene, Object* object,
                                                size_t index,
                                                const SceneSkeletonModifierDef& def) {
  if (object == nullptr) {
    return;
  }
  SkeletonModifier* modifier = object->getSkeletonModifierAt(index);
  if (modifier == nullptr) {
    return;
  }
  modifier->setEnabled(def.enabled);
  if (def.type == "PaperMouth") {
    auto* mouth = static_cast<SkeletonPaperMouthModifier*>(modifier);
    if (!def.bone_name.empty()) {
      mouth->setBoneName(def.bone_name);
    }
    mouth->setAttachDriven(def.attach_driven);
    mouth->setOpenAmount(def.open_amount);
  } else if (def.type == "SkeletonLookAtModifier") {
    auto* look_at = static_cast<SkeletonLookAtModifier*>(modifier);
    if (!def.bone_name.empty()) {
      look_at->setBoneName(def.bone_name);
    }
    look_at->setTarget(def.target);
  } else if (def.type == "SkeletonAttachModifier") {
    auto* attach = static_cast<SkeletonAttachModifier*>(modifier);
    if (!def.bone_name.empty()) {
      attach->setBoneName(def.bone_name);
    }
    if (scene != nullptr && !def.child_entity_name.empty()) {
      wireAttachChildFromScene(scene, object, index, def.child_entity_name);
    }
  }
}

inline bool restoreSkeletonModifierAt(SceneInstance* scene, Object* object,
                                      size_t index,
                                      const SceneSkeletonModifierDef& def) {
  if (object == nullptr) {
    return false;
  }
  eastl::unique_ptr<SkeletonModifier> modifier = makeSkeletonModifierFromDef(def);
  if (modifier == nullptr) {
    return false;
  }
  if (!object->insertSkeletonModifierAt(index, eastl::move(modifier))) {
    return false;
  }
  if (def.type == "SkeletonAttachModifier") {
    wireAttachChildFromScene(scene, object, index, def.child_entity_name);
  }
  return true;
}

inline void buildInspectorSkeletonModifierRows(
    const Object* object, const SceneInstance* scene,
    eastl::vector<InspectorSkeletonModifierRowData>& out_rows) {
  out_rows.clear();
  if (object == nullptr) {
    return;
  }
  const size_t count = object->getSkeletonModifierCount();
  out_rows.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    SceneSkeletonModifierDef def;
    if (scene != nullptr) {
      if (!captureSkeletonModifierDef(*scene, *object, i, def)) {
        continue;
      }
    } else {
      const SkeletonModifier* modifier = object->getSkeletonModifierAt(i);
      if (modifier == nullptr) {
        continue;
      }
      def.type = modifier->getTypeName();
      def.enabled = modifier->isEnabled();
      if (def.type == "PaperMouth") {
        const auto* mouth =
            static_cast<const SkeletonPaperMouthModifier*>(modifier);
        def.bone_name = mouth->getBoneName();
        def.open_amount = mouth->getOpenAmount();
        def.attach_driven = mouth->isAttachDriven();
      } else if (def.type == "SkeletonLookAtModifier") {
        const auto* look_at =
            static_cast<const SkeletonLookAtModifier*>(modifier);
        def.bone_name = look_at->getBoneName();
        def.target = look_at->getTarget();
      } else if (def.type == "SkeletonAttachModifier") {
        const auto* attach =
            static_cast<const SkeletonAttachModifier*>(modifier);
        def.bone_name = attach->getBoneName();
      }
    }

    InspectorSkeletonModifierRowData row{};
    row.index = i;
    row.type_name = def.type;
    row.enabled = def.enabled;
    row.bone_name = def.bone_name;
    row.open_amount = def.open_amount;
    row.attach_driven = def.attach_driven;
    row.target = def.target;
    row.child_entity_name = def.child_entity_name;
    out_rows.push_back(eastl::move(row));
  }
}

inline void buildSkeletonModifierTypeChoices(eastl::vector<eastl::string>& out) {
  out.clear();
  out.push_back("PaperMouth");
  out.push_back("SkeletonAttachModifier");
  out.push_back("SkeletonLookAtModifier");
}

}  // namespace Blunder
