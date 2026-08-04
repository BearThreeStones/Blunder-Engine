#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_skeleton_modifier_enabled(const void* instance) {
  return Variant(
      static_cast<const SkeletonModifier*>(instance)->isEnabled());
}

void set_skeleton_modifier_enabled(void* instance, const Variant& value) {
  static_cast<SkeletonModifier*>(instance)->setEnabled(value.asBool());
}

}  // namespace

void register_skeleton_modifier_reflection() {
  ClassDB::registerClass("SkeletonModifier");
  ClassDB::addProperty(
      "SkeletonModifier", PropertyInfo{"enabled", VariantType::Bool},
      set_skeleton_modifier_enabled, get_skeleton_modifier_enabled);
}

}  // namespace Blunder
