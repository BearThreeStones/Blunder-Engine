#include "EASTL/unique_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/missing_skeleton_modifier.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier_catalog.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"

#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

class CatalogTestDouble final : public Blunder::SkeletonModifier {
 public:
  const char* getTypeName() const override { return "CatalogTestDouble"; }
};

eastl::unique_ptr<Blunder::SkeletonModifier> makeCatalogTestDouble() {
  return eastl::make_unique<CatalogTestDouble>();
}

void test_construct_product_types() {
  using namespace Blunder;
  ClassDB::initialize();

  eastl::unique_ptr<SkeletonModifier> mouth =
      SkeletonModifierCatalog::construct("PaperMouth");
  expect_true("construct PaperMouth", mouth != nullptr);
  expect_true("PaperMouth type",
              mouth != nullptr &&
                  std::strcmp(mouth->getTypeName(), "PaperMouth") == 0);

  eastl::unique_ptr<SkeletonModifier> attach =
      SkeletonModifierCatalog::construct("SkeletonAttachModifier");
  expect_true("construct Attach", attach != nullptr);
  expect_true(
      "Attach type",
      attach != nullptr &&
          std::strcmp(attach->getTypeName(), "SkeletonAttachModifier") == 0);

  eastl::unique_ptr<SkeletonModifier> look_at =
      SkeletonModifierCatalog::construct("SkeletonLookAtModifier");
  expect_true("construct LookAt", look_at != nullptr);
  expect_true(
      "LookAt type",
      look_at != nullptr &&
          std::strcmp(look_at->getTypeName(), "SkeletonLookAtModifier") == 0);

  ClassDB::shutdown();
}

void test_initialize_twice_no_duplicate_add_menu() {
  using namespace Blunder;
  ClassDB::initialize();
  ClassDB::initialize();

  eastl::vector<eastl::string> names;
  SkeletonModifierCatalog::listAddMenuTypes(names);
  expect_true("three Add… product types", names.size() == 3);
  bool saw_base = false;
  for (const eastl::string& name : names) {
    if (name == "SkeletonModifier") {
      saw_base = true;
    }
  }
  expect_true("base type not addable", !saw_base);
  expect_true("has PaperMouth", SkeletonModifierCatalog::hasType("PaperMouth"));

  ClassDB::shutdown();
}

void test_reversible_test_double() {
  using namespace Blunder;
  ClassDB::initialize();

  {
    SkeletonModifierTypeRegistration registration =
        SkeletonModifierCatalog::registerType("CatalogTestDouble",
                                              &makeCatalogTestDouble, false);
    expect_true("registered", static_cast<bool>(registration));
    eastl::unique_ptr<SkeletonModifier> instance =
        SkeletonModifierCatalog::construct("CatalogTestDouble");
    expect_true("construct test double", instance != nullptr);
    expect_true("test double type",
                instance != nullptr && std::strcmp(instance->getTypeName(),
                                                   "CatalogTestDouble") == 0);

    eastl::vector<eastl::string> names;
    SkeletonModifierCatalog::listAddMenuTypes(names);
    bool listed = false;
    for (const eastl::string& name : names) {
      if (name == "CatalogTestDouble") {
        listed = true;
      }
    }
    expect_true("hidden from Add…", !listed);
  }

  expect_true("unregistered construct fails",
              SkeletonModifierCatalog::construct("CatalogTestDouble") ==
                  nullptr);
  expect_true("unregistered hasType false",
              !SkeletonModifierCatalog::hasType("CatalogTestDouble"));

  ClassDB::shutdown();
}

void test_unknown_type_missing_round_trip() {
  using namespace Blunder;
  ClassDB::initialize();
  ObjectDB::clear();

  const char* kJson = R"({
  "type": "Scene",
  "guid": "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee",
  "entities": [
    {
      "name": "Rig",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "skeletonModifiers": [
        {
          "type": "IkTwoBone",
          "enabled": true,
          "hint": "keep-me",
          "weight": 0.5,
          "chain": { "bones": ["A", "B"] }
        }
      ]
    }
  ]
}
)";

  Scene loaded;
  expect_true("deserialize unknown modifier",
              SceneSerializer::deserialize(eastl::string(kJson), loaded,
                                           nullptr));
  expect_true("one entity", loaded.getEntities().size() == 1);
  if (loaded.getEntities().empty()) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }
  expect_true("one modifier def",
              loaded.getEntities()[0].skeleton_modifiers.size() == 1);
  if (loaded.getEntities()[0].skeleton_modifiers.empty()) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }
  const SceneSkeletonModifierDef& def =
      loaded.getEntities()[0].skeleton_modifiers[0];
  expect_true("def type IkTwoBone", def.type == "IkTwoBone");
  expect_true("extra fields kept", def.extra_fields.size() >= 3);

  SceneInstance instance;
  instance.instantiate(loaded);
  const EntityId rig = instance.findEntityByName("Rig");
  Object* object = instance.findBoundObject(rig);
  expect_true("bound object", object != nullptr);
  if (object == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }
  expect_true("one live slot", object->getSkeletonModifierCount() == 1);
  SkeletonModifier* slot = object->getSkeletonModifierAt(0);
  expect_true("missing slot", slot != nullptr && slot->isMissing());
  expect_true("authored type kept",
              slot != nullptr &&
                  std::strcmp(slot->getTypeName(), "IkTwoBone") == 0);
  expect_true("not coerced to base",
              slot != nullptr &&
                  std::strcmp(slot->getTypeName(), "SkeletonModifier") != 0);

  Skeleton* skeleton = object->ensureSkeleton();
  const int bone = skeleton->addBone("Root", -1);
  skeleton->resetPoseToRest();
  const BoneTransform rest = skeleton->getBonePoseLocal(static_cast<size_t>(bone));
  object->applySkeletonModifiers(*skeleton);
  const BoneTransform after =
      skeleton->getBonePoseLocal(static_cast<size_t>(bone));
  expect_true("missing apply is no-op",
              rest.translation.x == after.translation.x &&
                  rest.translation.y == after.translation.y &&
                  rest.translation.z == after.translation.z);

  Scene exported;
  expect_true("export", instance.exportToScene(exported));
  eastl::string json;
  expect_true("save succeeds",
              SceneSerializer::serialize(exported, json, nullptr));
  expect_true("saved type", json.find("\"IkTwoBone\"") != eastl::string::npos);
  expect_true("saved hint", json.find("keep-me") != eastl::string::npos);
  expect_true("saved weight", json.find("0.5") != eastl::string::npos);
  expect_true("saved nested chain", json.find("\"bones\"") != eastl::string::npos);

  ObjectDB::clear();
  ClassDB::shutdown();
}

void test_add_menu_three_product_names() {
  using namespace Blunder;
  ClassDB::initialize();
  eastl::vector<eastl::string> names;
  SkeletonModifierCatalog::listAddMenuTypes(names);
  expect_true("Add… count 3", names.size() == 3);
  expect_true("Add… [0] PaperMouth",
              names.size() >= 1 && names[0] == "PaperMouth");
  expect_true("Add… [1] Attach",
              names.size() >= 2 && names[1] == "SkeletonAttachModifier");
  expect_true("Add… [2] LookAt",
              names.size() >= 3 && names[2] == "SkeletonLookAtModifier");
  ClassDB::shutdown();
}

}  // namespace

int main() {
  test_construct_product_types();
  test_initialize_twice_no_duplicate_add_menu();
  test_reversible_test_double();
  test_unknown_type_missing_round_trip();
  test_add_menu_three_product_names();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
