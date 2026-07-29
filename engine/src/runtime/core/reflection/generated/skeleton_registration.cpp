#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_skeleton_bone_count(const void* instance) {
  return Variant(static_cast<int64_t>(
      static_cast<const Skeleton*>(instance)->getBoneCount()));
}

}  // namespace

void register_skeleton_reflection() {
  ClassDB::registerClass("Skeleton");
  ClassDB::addProperty("Skeleton", PropertyInfo{"bone_count", VariantType::Int},
                       nullptr, get_skeleton_bone_count);
}

}  // namespace Blunder
