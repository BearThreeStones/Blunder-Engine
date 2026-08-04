#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_attach_bone_name(const void* instance) {
  return Variant(
      static_cast<const SkeletonAttachModifier*>(instance)->getBoneName());
}

void set_attach_bone_name(void* instance, const Variant& value) {
  static_cast<SkeletonAttachModifier*>(instance)->setBoneName(value.asString());
}

Variant get_attach_child_object_id(const void* instance) {
  return Variant(static_cast<int64_t>(
      static_cast<const SkeletonAttachModifier*>(instance)->getChildObjectId()));
}

void set_attach_child_object_id(void* instance, const Variant& value) {
  static_cast<SkeletonAttachModifier*>(instance)->setChildObjectId(
      static_cast<ObjectId>(value.asInt()));
}

}  // namespace

void register_skeleton_attach_modifier_reflection() {
  ClassDB::registerClass("SkeletonAttachModifier", "SkeletonModifier");
  ClassDB::addProperty(
      "SkeletonAttachModifier", PropertyInfo{"bone_name", VariantType::String},
      set_attach_bone_name, get_attach_bone_name);
  ClassDB::addProperty(
      "SkeletonAttachModifier",
      PropertyInfo{"child_object_id", VariantType::Int},
      set_attach_child_object_id, get_attach_child_object_id);
}

}  // namespace Blunder
