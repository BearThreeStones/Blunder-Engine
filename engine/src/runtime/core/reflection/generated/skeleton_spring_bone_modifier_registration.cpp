#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/skeleton_spring_bone_modifier.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_spring_bone_name(const void* instance) {
  return Variant(
      static_cast<const SkeletonSpringBoneModifier*>(instance)->getRootBoneName());
}

void set_spring_bone_name(void* instance, const Variant& value) {
  static_cast<SkeletonSpringBoneModifier*>(instance)->setRootBoneName(
      value.asString());
}

Variant get_spring_end_bone_name(const void* instance) {
  return Variant(
      static_cast<const SkeletonSpringBoneModifier*>(instance)->getEndBoneName());
}

void set_spring_end_bone_name(void* instance, const Variant& value) {
  static_cast<SkeletonSpringBoneModifier*>(instance)->setEndBoneName(
      value.asString());
}

Variant get_spring_stiffness(const void* instance) {
  return Variant(
      static_cast<const SkeletonSpringBoneModifier*>(instance)->getStiffness());
}

void set_spring_stiffness(void* instance, const Variant& value) {
  static_cast<SkeletonSpringBoneModifier*>(instance)->setStiffness(
      value.asFloat());
}

Variant get_spring_drag(const void* instance) {
  return Variant(
      static_cast<const SkeletonSpringBoneModifier*>(instance)->getDrag());
}

void set_spring_drag(void* instance, const Variant& value) {
  static_cast<SkeletonSpringBoneModifier*>(instance)->setDrag(value.asFloat());
}

Variant get_spring_gravity_x(const void* instance) {
  return Variant(
      static_cast<const SkeletonSpringBoneModifier*>(instance)->getGravity().x);
}

void set_spring_gravity_x(void* instance, const Variant& value) {
  SkeletonSpringBoneModifier* modifier =
      static_cast<SkeletonSpringBoneModifier*>(instance);
  Vec3 gravity = modifier->getGravity();
  gravity.x = value.asFloat();
  modifier->setGravity(gravity);
}

Variant get_spring_gravity_y(const void* instance) {
  return Variant(
      static_cast<const SkeletonSpringBoneModifier*>(instance)->getGravity().y);
}

void set_spring_gravity_y(void* instance, const Variant& value) {
  SkeletonSpringBoneModifier* modifier =
      static_cast<SkeletonSpringBoneModifier*>(instance);
  Vec3 gravity = modifier->getGravity();
  gravity.y = value.asFloat();
  modifier->setGravity(gravity);
}

Variant get_spring_gravity_z(const void* instance) {
  return Variant(
      static_cast<const SkeletonSpringBoneModifier*>(instance)->getGravity().z);
}

void set_spring_gravity_z(void* instance, const Variant& value) {
  SkeletonSpringBoneModifier* modifier =
      static_cast<SkeletonSpringBoneModifier*>(instance);
  Vec3 gravity = modifier->getGravity();
  gravity.z = value.asFloat();
  modifier->setGravity(gravity);
}

Variant get_spring_end_bone_length(const void* instance) {
  return Variant(static_cast<const SkeletonSpringBoneModifier*>(instance)
                     ->getEndBoneLength());
}

void set_spring_end_bone_length(void* instance, const Variant& value) {
  static_cast<SkeletonSpringBoneModifier*>(instance)->setEndBoneLength(
      value.asFloat());
}

}  // namespace

void register_skeleton_spring_bone_modifier_reflection() {
  ClassDB::registerClass("SpringBone", "SkeletonModifier");
  ClassDB::addProperty("SpringBone",
                       PropertyInfo{"bone_name", VariantType::String},
                       set_spring_bone_name, get_spring_bone_name);
  ClassDB::addProperty("SpringBone",
                       PropertyInfo{"end_bone_name", VariantType::String},
                       set_spring_end_bone_name, get_spring_end_bone_name);
  ClassDB::addProperty("SpringBone",
                       PropertyInfo{"stiffness", VariantType::Float},
                       set_spring_stiffness, get_spring_stiffness);
  ClassDB::addProperty("SpringBone", PropertyInfo{"drag", VariantType::Float},
                       set_spring_drag, get_spring_drag);
  ClassDB::addProperty("SpringBone",
                       PropertyInfo{"gravity_x", VariantType::Float},
                       set_spring_gravity_x, get_spring_gravity_x);
  ClassDB::addProperty("SpringBone",
                       PropertyInfo{"gravity_y", VariantType::Float},
                       set_spring_gravity_y, get_spring_gravity_y);
  ClassDB::addProperty("SpringBone",
                       PropertyInfo{"gravity_z", VariantType::Float},
                       set_spring_gravity_z, get_spring_gravity_z);
  ClassDB::addProperty("SpringBone",
                       PropertyInfo{"end_bone_length", VariantType::Float},
                       set_spring_end_bone_length, get_spring_end_bone_length);
}

}  // namespace Blunder
