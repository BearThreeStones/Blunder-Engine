#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/engine_c_abi.h"

#include <cmath>
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

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_true("abi version is 10",
              BLUNDER_ENGINE_C_ABI_VERSION == 10);
  expect_true("abi version callable",
              blunder_engine_abi_version() == BLUNDER_ENGINE_C_ABI_VERSION);

  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  expect_true("host created", host != nullptr);
  expect_true("child created", child != nullptr);
  if (host == nullptr || child == nullptr) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
  }

  const BlunderObjectId host_abi_id = static_cast<BlunderObjectId>(host_id);
  const BlunderObjectId child_abi_id = static_cast<BlunderObjectId>(child_id);

  SkeletonPaperMouthModifier* paper_mouth = host->addSkeletonPaperMouthModifier();
  SkeletonAttachModifier* attach = host->addSkeletonAttachModifier();
  SkeletonLookAtModifier* look_at = host->addSkeletonLookAtModifier();
  expect_true("paper mouth added", paper_mouth != nullptr);
  expect_true("attach added", attach != nullptr);
  expect_true("look at added", look_at != nullptr);

  expect_true("set paper mouth open amount",
              blunder_skeleton_modifier_set_paper_mouth_open_amount(
                  host_abi_id, 0, 0.75f) == BLUNDER_ENGINE_OK);
  float open_amount = 0.0f;
  expect_true("get paper mouth open amount",
              blunder_skeleton_modifier_get_paper_mouth_open_amount(
                  host_abi_id, 0, &open_amount) == BLUNDER_ENGINE_OK);
  expect_true("paper mouth open amount value", float_near(open_amount, 0.75f));

  expect_true("set paper mouth bone name",
              blunder_skeleton_modifier_set_paper_mouth_bone_name(
                  host_abi_id, 0, "Jaw") == BLUNDER_ENGINE_OK);
  char bone_name[64] = {};
  expect_true("get paper mouth bone name",
              blunder_skeleton_modifier_get_paper_mouth_bone_name(
                  host_abi_id, 0, bone_name,
                  static_cast<int>(sizeof(bone_name))) == BLUNDER_ENGINE_OK);
  expect_true("paper mouth bone name value", std::strcmp(bone_name, "Jaw") == 0);

  expect_true("set attach bone name",
              blunder_skeleton_modifier_set_attach_bone_name(host_abi_id, 1,
                                                             "Hand") ==
                  BLUNDER_ENGINE_OK);
  bone_name[0] = '\0';
  expect_true("get attach bone name",
              blunder_skeleton_modifier_get_attach_bone_name(
                  host_abi_id, 1, bone_name,
                  static_cast<int>(sizeof(bone_name))) == BLUNDER_ENGINE_OK);
  expect_true("attach bone name value", std::strcmp(bone_name, "Hand") == 0);

  expect_true("set attach child object id",
              blunder_skeleton_modifier_set_attach_child_object_id(
                  host_abi_id, 1, child_abi_id) == BLUNDER_ENGINE_OK);
  BlunderObjectId read_child_id = 0;
  expect_true("get attach child object id",
              blunder_skeleton_modifier_get_attach_child_object_id(
                  host_abi_id, 1, &read_child_id) == BLUNDER_ENGINE_OK);
  expect_true("attach child object id value", read_child_id == child_abi_id);

  expect_true("set look at target",
              blunder_skeleton_modifier_set_look_at_target(host_abi_id, 2, 1.0f,
                                                           2.0f, 3.0f) ==
                  BLUNDER_ENGINE_OK);
  float target_x = 0.0f;
  float target_y = 0.0f;
  float target_z = 0.0f;
  expect_true("get look at target",
              blunder_skeleton_modifier_get_look_at_target(
                  host_abi_id, 2, &target_x, &target_y, &target_z) ==
                  BLUNDER_ENGINE_OK);
  expect_true("look at target x", float_near(target_x, 1.0f));
  expect_true("look at target y", float_near(target_y, 2.0f));
  expect_true("look at target z", float_near(target_z, 3.0f));

  expect_true("set look at bone name",
              blunder_skeleton_modifier_set_look_at_bone_name(host_abi_id, 2,
                                                              "Head") ==
                  BLUNDER_ENGINE_OK);
  bone_name[0] = '\0';
  expect_true("get look at bone name",
              blunder_skeleton_modifier_get_look_at_bone_name(
                  host_abi_id, 2, bone_name,
                  static_cast<int>(sizeof(bone_name))) == BLUNDER_ENGINE_OK);
  expect_true("look at bone name value", std::strcmp(bone_name, "Head") == 0);

  expect_true("wrong type for paper mouth open amount",
              blunder_skeleton_modifier_set_paper_mouth_open_amount(
                  host_abi_id, 1, 0.5f) == BLUNDER_ENGINE_ERROR);
  expect_true("wrong type for attach bone name",
              blunder_skeleton_modifier_set_attach_bone_name(host_abi_id, 0,
                                                             "Hand") ==
                  BLUNDER_ENGINE_ERROR);
  expect_true("wrong type for look at target",
              blunder_skeleton_modifier_set_look_at_target(host_abi_id, 0, 0.0f,
                                                           0.0f, 0.0f) ==
                  BLUNDER_ENGINE_ERROR);

  expect_true("invalid object",
              blunder_skeleton_modifier_set_paper_mouth_open_amount(0, 0, 1.0f) ==
                  BLUNDER_ENGINE_ERROR);
  expect_true("invalid index",
              blunder_skeleton_modifier_set_paper_mouth_open_amount(
                  host_abi_id, 99, 1.0f) == BLUNDER_ENGINE_ERROR);

  BlunderNativeAbi abi{};
  blunder_native_abi_fill_from_process(&abi);
  expect_true("table set paper mouth open amount",
              abi.skeleton_modifier_set_paper_mouth_open_amount != nullptr);
  expect_true("table get attach child object id",
              abi.skeleton_modifier_get_attach_child_object_id != nullptr);
  expect_true("table set look at target",
              abi.skeleton_modifier_set_look_at_target != nullptr);
  expect_true("table via set open amount",
              abi.skeleton_modifier_set_paper_mouth_open_amount(host_abi_id, 0,
                                                                0.25f) ==
                  BLUNDER_ENGINE_OK);
  open_amount = 0.0f;
  expect_true("table via get open amount",
              abi.skeleton_modifier_get_paper_mouth_open_amount(
                  host_abi_id, 0, &open_amount) == BLUNDER_ENGINE_OK);
  expect_true("table open amount value", float_near(open_amount, 0.25f));

  ObjectDB::destroy(host_id);
  ObjectDB::destroy(child_id);
  ObjectDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }

  std::printf("skeleton_modifier_product_c_abi_test: all passed\n");
  return 0;
}
