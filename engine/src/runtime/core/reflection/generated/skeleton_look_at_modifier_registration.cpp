#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_look_at_bone_name(const void* instance) {
  return Variant(
      static_cast<const SkeletonLookAtModifier*>(instance)->getBoneName());
}

void set_look_at_bone_name(void* instance, const Variant& value) {
  static_cast<SkeletonLookAtModifier*>(instance)->setBoneName(value.asString());
}

Variant get_look_at_target_x(const void* instance) {
  return Variant(
      static_cast<const SkeletonLookAtModifier*>(instance)->getTarget().x);
}

void set_look_at_target_x(void* instance, const Variant& value) {
  SkeletonLookAtModifier* modifier = static_cast<SkeletonLookAtModifier*>(instance);
  Vec3 target = modifier->getTarget();
  target.x = value.asFloat();
  modifier->setTarget(target);
}

Variant get_look_at_target_y(const void* instance) {
  return Variant(
      static_cast<const SkeletonLookAtModifier*>(instance)->getTarget().y);
}

void set_look_at_target_y(void* instance, const Variant& value) {
  SkeletonLookAtModifier* modifier = static_cast<SkeletonLookAtModifier*>(instance);
  Vec3 target = modifier->getTarget();
  target.y = value.asFloat();
  modifier->setTarget(target);
}

Variant get_look_at_target_z(const void* instance) {
  return Variant(
      static_cast<const SkeletonLookAtModifier*>(instance)->getTarget().z);
}

void set_look_at_target_z(void* instance, const Variant& value) {
  SkeletonLookAtModifier* modifier = static_cast<SkeletonLookAtModifier*>(instance);
  Vec3 target = modifier->getTarget();
  target.z = value.asFloat();
  modifier->setTarget(target);
}

}  // namespace

void register_skeleton_look_at_modifier_reflection() {
  ClassDB::registerClass("SkeletonLookAtModifier", "SkeletonModifier");
  ClassDB::addProperty(
      "SkeletonLookAtModifier", PropertyInfo{"bone_name", VariantType::String},
      set_look_at_bone_name, get_look_at_bone_name);
  ClassDB::addProperty(
      "SkeletonLookAtModifier", PropertyInfo{"target_x", VariantType::Float},
      set_look_at_target_x, get_look_at_target_x);
  ClassDB::addProperty(
      "SkeletonLookAtModifier", PropertyInfo{"target_y", VariantType::Float},
      set_look_at_target_y, get_look_at_target_y);
  ClassDB::addProperty(
      "SkeletonLookAtModifier", PropertyInfo{"target_z", VariantType::Float},
      set_look_at_target_z, get_look_at_target_z);
}

}  // namespace Blunder
