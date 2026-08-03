#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_animation_tree_has_animation_player(const void* instance) {
  return Variant(
      static_cast<const AnimationTree*>(instance)->hasAnimationPlayer());
}

Variant get_animation_tree_active(const void* instance) {
  return Variant(static_cast<const AnimationTree*>(instance)->isActive());
}

void set_animation_tree_active(void* instance, const Variant& value) {
  static_cast<AnimationTree*>(instance)->setActive(value.asBool());
}

Variant get_animation_tree_current_state(const void* instance) {
  return Variant(
      static_cast<const AnimationTree*>(instance)->getCurrentStateName());
}

void set_animation_tree_current_state(void* instance, const Variant& value) {
  static_cast<AnimationTree*>(instance)->travel(value.asString());
}

Variant get_animation_tree_add2_clip(const void* instance) {
  return Variant(
      static_cast<const AnimationTree*>(instance)->getAdd2ClipName());
}

void set_animation_tree_add2_clip(void* instance, const Variant& value) {
  static_cast<AnimationTree*>(instance)->setAdd2ClipName(value.asString());
}

Variant get_animation_tree_add2_weight(const void* instance) {
  return Variant(static_cast<const AnimationTree*>(instance)->getAdd2Weight());
}

void set_animation_tree_add2_weight(void* instance, const Variant& value) {
  static_cast<AnimationTree*>(instance)->setAdd2Weight(value.asFloat());
}

Variant get_animation_tree_oneshot_slot_clip(const void* instance) {
  return Variant(
      static_cast<const AnimationTree*>(instance)->getOneShotSlotClip());
}

void set_animation_tree_oneshot_slot_clip(void* instance, const Variant& value) {
  static_cast<AnimationTree*>(instance)->setOneShotSlotClip(value.asString());
}

class MethodBindTravel final : public MethodBind {
 public:
  void ptrcall(void* instance, const void** args, void* ret) override {
    const Variant* arg = static_cast<const Variant*>(args[0]);
    const bool ok =
        static_cast<AnimationTree*>(instance)->travel(arg->asString());
    if (ret != nullptr) {
      *static_cast<Variant*>(ret) = Variant(ok);
    }
  }
  const char* getName() const override { return "travel"; }
};

class MethodBindStart final : public MethodBind {
 public:
  void ptrcall(void* instance, const void** args, void* ret) override {
    const Variant* arg = static_cast<const Variant*>(args[0]);
    const bool ok =
        static_cast<AnimationTree*>(instance)->start(arg->asString());
    if (ret != nullptr) {
      *static_cast<Variant*>(ret) = Variant(ok);
    }
  }
  const char* getName() const override { return "start"; }
};

class MethodBindSetBlendSpaceScalar final : public MethodBind {
 public:
  void ptrcall(void* instance, const void** args, void* ret) override {
    const Variant* node_arg = static_cast<const Variant*>(args[0]);
    const Variant* scalar_arg = static_cast<const Variant*>(args[1]);
    static_cast<AnimationTree*>(instance)->setBlendSpaceScalar(
        node_arg->asString(), scalar_arg->asFloat());
    if (ret != nullptr) {
      *static_cast<Variant*>(ret) = Variant(true);
    }
  }
  const char* getName() const override { return "set_blend_space_scalar"; }
};

class MethodBindRequestOneShot final : public MethodBind {
 public:
  void ptrcall(void* instance, const void** args, void* ret) override {
    const Variant* arg = static_cast<const Variant*>(args[0]);
    const bool ok =
        static_cast<AnimationTree*>(instance)->requestOneShot(arg->asString());
    if (ret != nullptr) {
      *static_cast<Variant*>(ret) = Variant(ok);
    }
  }
  const char* getName() const override { return "request_oneshot"; }
};

}  // namespace

void register_animation_tree_reflection() {
  ClassDB::registerClass("AnimationTree");
  ClassDB::addProperty(
      "AnimationTree",
      PropertyInfo{"has_animation_player", VariantType::Bool}, nullptr,
      get_animation_tree_has_animation_player);
  ClassDB::addProperty("AnimationTree", PropertyInfo{"active", VariantType::Bool},
                       set_animation_tree_active, get_animation_tree_active);
  ClassDB::addProperty(
      "AnimationTree", PropertyInfo{"current_state", VariantType::String},
      set_animation_tree_current_state, get_animation_tree_current_state);
  ClassDB::addProperty(
      "AnimationTree", PropertyInfo{"add2_clip", VariantType::String},
      set_animation_tree_add2_clip, get_animation_tree_add2_clip);
  ClassDB::addProperty(
      "AnimationTree", PropertyInfo{"add2_weight", VariantType::Float},
      set_animation_tree_add2_weight, get_animation_tree_add2_weight);
  ClassDB::addProperty(
      "AnimationTree", PropertyInfo{"oneshot_slot_clip", VariantType::String},
      set_animation_tree_oneshot_slot_clip,
      get_animation_tree_oneshot_slot_clip);
  ClassDB::addMethod("AnimationTree", new MethodBindTravel());
  ClassDB::addMethod("AnimationTree", new MethodBindStart());
  ClassDB::addMethod("AnimationTree", new MethodBindSetBlendSpaceScalar());
  ClassDB::addMethod("AnimationTree", new MethodBindRequestOneShot());
}

}  // namespace Blunder
