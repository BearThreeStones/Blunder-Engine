#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_animation_tree_has_animation_player(const void* instance) {
  return Variant(
      static_cast<const AnimationTree*>(instance)->hasAnimationPlayer());
}

}  // namespace

void register_animation_tree_reflection() {
  ClassDB::registerClass("AnimationTree");
  ClassDB::addProperty(
      "AnimationTree",
      PropertyInfo{"has_animation_player", VariantType::Bool}, nullptr,
      get_animation_tree_has_animation_player);
}

}  // namespace Blunder
