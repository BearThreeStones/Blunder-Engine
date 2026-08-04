#include "runtime/core/reflection/class_db.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/generated/register_generated.h"

namespace Blunder {
namespace {

Variant get_paper_mouth_open_amount(const void* instance) {
  return Variant(
      static_cast<const SkeletonPaperMouthModifier*>(instance)->getOpenAmount());
}

void set_paper_mouth_open_amount(void* instance, const Variant& value) {
  static_cast<SkeletonPaperMouthModifier*>(instance)->setOpenAmount(
      value.asFloat());
}

Variant get_paper_mouth_bone_name(const void* instance) {
  return Variant(
      static_cast<const SkeletonPaperMouthModifier*>(instance)->getBoneName());
}

void set_paper_mouth_bone_name(void* instance, const Variant& value) {
  static_cast<SkeletonPaperMouthModifier*>(instance)->setBoneName(
      value.asString());
}

Variant get_paper_mouth_attach_driven(const void* instance) {
  return Variant(
      static_cast<const SkeletonPaperMouthModifier*>(instance)->isAttachDriven());
}

void set_paper_mouth_attach_driven(void* instance, const Variant& value) {
  static_cast<SkeletonPaperMouthModifier*>(instance)->setAttachDriven(
      value.asBool());
}

}  // namespace

void register_skeleton_paper_mouth_modifier_reflection() {
  ClassDB::registerClass("PaperMouth", "SkeletonModifier");
  ClassDB::addProperty(
      "PaperMouth", PropertyInfo{"open_amount", VariantType::Float},
      set_paper_mouth_open_amount, get_paper_mouth_open_amount);
  ClassDB::addProperty(
      "PaperMouth", PropertyInfo{"bone_name", VariantType::String},
      set_paper_mouth_bone_name, get_paper_mouth_bone_name);
  ClassDB::addProperty(
      "PaperMouth", PropertyInfo{"attach_driven", VariantType::Bool},
      set_paper_mouth_attach_driven, get_paper_mouth_attach_driven);
}

}  // namespace Blunder
