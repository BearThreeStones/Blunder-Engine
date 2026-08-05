#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/reflection/class_db.h"

#include <cmath>
#include <cstdio>

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

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = 1e-4f) {
  return float_near(a.x, b.x, eps) && float_near(a.y, b.y, eps) &&
         float_near(a.z, b.z, eps);
}

bool quat_near(const Blunder::Quat& a, const Blunder::Quat& b,
               float eps = 1e-4f) {
  const float dot = std::fabs(glm::dot(a, b));
  return dot > 1.0f - eps || dot < -1.0f + eps;
}

void decompose_bone_matrix(const Blunder::Mat4& matrix, Blunder::Vec3& position,
                           Blunder::Quat& rotation, Blunder::Vec3& scale) {
  position = Blunder::Vec3(matrix[3]);
  Blunder::Vec3 col0(matrix[0]);
  Blunder::Vec3 col1(matrix[1]);
  Blunder::Vec3 col2(matrix[2]);
  scale.x = glm::length(col0);
  scale.y = glm::length(col1);
  scale.z = glm::length(col2);
  if (scale.x > 1e-8f) {
    col0 /= scale.x;
  }
  if (scale.y > 1e-8f) {
    col1 /= scale.y;
  }
  if (scale.z > 1e-8f) {
    col2 /= scale.z;
  }
  const Blunder::Mat3 rot_mat(col0, col1, col2);
  rotation = glm::quat_cast(Blunder::Mat4(rot_mat));
}

bool object_transform_matches_bone(const Blunder::Object& object,
                                   const Blunder::Skeleton& skeleton,
                                   size_t bone_index) {
  const Blunder::Mat4 bone_global =
      skeleton.getBoneGlobalPoseMatrix(bone_index);
  Blunder::Vec3 expected_position;
  Blunder::Quat expected_rotation;
  Blunder::Vec3 expected_scale;
  decompose_bone_matrix(bone_global, expected_position, expected_rotation,
                        expected_scale);
  return vec3_near(object.getPosition(), expected_position) &&
         quat_near(object.getRotation(), expected_rotation) &&
         vec3_near(object.getScale(), expected_scale);
}

Blunder::AnimationTrack makeTranslationTrack(
    const char* bone, Blunder::AnimationInterpolation interpolation,
    std::initializer_list<std::pair<float, Blunder::Vec3>> keys) {
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = Blunder::AnimationChannel::Translation;
  track.interpolation = interpolation;
  for (const auto& key : keys) {
    Blunder::AnimationKeyframe frame;
    frame.time = key.first;
    frame.value = {key.second.x, key.second.y, key.second.z};
    track.keys.push_back(frame);
  }
  return track;
}

/// Task 3.1: host bone world transform copies to child Object Transform.
void test_attach_modifier_copies_bone_world_to_child_transform() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("SkeletonAttachModifier registered",
              ClassDB::hasClass("SkeletonAttachModifier"));

  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  expect_true("host created", host != nullptr);
  expect_true("child created", child != nullptr);
  if (host == nullptr || child == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  child->setParent(host);

  Skeleton* skeleton = host->ensureSkeleton();
  const int hand = skeleton->addBone("Hand", -1);
  skeleton->setBoneRestLocal(static_cast<size_t>(hand),
                             BoneTransform{Vec3(0.2f, 0.5f, 1.0f),
                                           glm::angleAxis(0.3f, Vec3(0.0f, 1.0f, 0.0f)),
                                           Vec3(1.0f, 1.0f, 1.0f)});
  skeleton->resetPoseToRest();

  SkeletonAttachModifier* attach = host->addSkeletonAttachModifier();
  expect_true("attach modifier created", attach != nullptr);
  if (attach == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  expect_true("bone_name set via ClassDB",
              ClassDB::setProperty(attach, "SkeletonAttachModifier", "bone_name",
                                   Variant(eastl::string("Hand"))));
  expect_true("child_object_id set via ClassDB",
              ClassDB::setProperty(attach, "SkeletonAttachModifier",
                                   "child_object_id",
                                   Variant(static_cast<int64_t>(child_id))));

  Variant bone_name;
  expect_true("bone_name get via ClassDB",
              ClassDB::getProperty(attach, "SkeletonAttachModifier", "bone_name",
                                   bone_name));
  expect_true("bone_name round-trip", bone_name.asString() == "Hand");

  Variant child_id_value;
  expect_true("child_object_id get via ClassDB",
              ClassDB::getProperty(attach, "SkeletonAttachModifier",
                                   "child_object_id", child_id_value));
  expect_true("child_object_id round-trip",
              static_cast<ObjectId>(child_id_value.asInt()) == child_id);

  attach->apply(*skeleton);
  expect_true("child transform matches bone world after apply",
              object_transform_matches_bone(*child, *skeleton,
                                            static_cast<size_t>(hand)));

  BoneTransform pose = skeleton->getBonePoseLocal(static_cast<size_t>(hand));
  pose.translation = Vec3(0.7f, -0.2f, 0.4f);
  pose.rotation = glm::angleAxis(1.1f, Vec3(0.0f, 0.0f, 1.0f));
  skeleton->setBonePoseLocal(static_cast<size_t>(hand), pose);
  attach->apply(*skeleton);
  expect_true("child follows updated bone pose",
              object_transform_matches_bone(*child, *skeleton,
                                            static_cast<size_t>(hand)));

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 3.1: attach runs in modifier chain after AnimationPlayer sample.
void test_attach_modifier_after_sample() {
  using namespace Blunder;

  ClassDB::initialize();

  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  expect_true("host created", host != nullptr);
  expect_true("child created", child != nullptr);
  if (host == nullptr || child == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  child->setParent(host);

  Skeleton* skeleton = host->ensureSkeleton();
  AnimationPlayer* player = host->ensureAnimationPlayer();
  const int hand = skeleton->addBone("Hand", -1);
  skeleton->resetPoseToRest();

  SkeletonAttachModifier* attach = host->addSkeletonAttachModifier();
  expect_true("attach modifier created", attach != nullptr);
  if (attach == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  attach->setBoneName("Hand");
  attach->setChildObjectId(child_id);

  const eastl::string guid = "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hand", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.1f, 0.2f, 0.3f)}, {1.0f, Vec3(0.1f, 0.2f, 0.3f)}}));
  player->setClipGuid("pose", guid);
  player->injectClipData(guid, clip);

  const Vec3 child_position_before = child->getPosition();
  expect_true("play", player->play("pose"));
  expect_true("child position changed after sample + attach",
              !vec3_near(child->getPosition(), child_position_before));
  expect_true("child transform matches sampled bone world",
              object_transform_matches_bone(*child, *skeleton,
                                            static_cast<size_t>(hand)));

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 3.2: invalid child ObjectId skips transform write without crash.
void test_invalid_child_id_skips_transform_write() {
  using namespace Blunder;

  ClassDB::initialize();

  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  expect_true("host created", host != nullptr);
  expect_true("child created", child != nullptr);
  if (host == nullptr || child == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  child->setParent(host);
  child->setPosition(Vec3(1.0f, 2.0f, 3.0f));
  child->setRotation(glm::angleAxis(0.5f, Vec3(0.0f, 1.0f, 0.0f)));
  child->setScale(Vec3(2.0f, 2.0f, 2.0f));
  const Vec3 position_before = child->getPosition();
  const Quat rotation_before = child->getRotation();
  const Vec3 scale_before = child->getScale();

  Skeleton* skeleton = host->ensureSkeleton();
  const int hand = skeleton->addBone("Hand", -1);
  skeleton->setBoneRestLocal(static_cast<size_t>(hand),
                             BoneTransform{Vec3(0.2f, 0.5f, 1.0f),
                                           glm::identity<Quat>(),
                                           Vec3(1.0f, 1.0f, 1.0f)});
  skeleton->resetPoseToRest();

  SkeletonAttachModifier* attach = host->addSkeletonAttachModifier();
  expect_true("attach modifier created", attach != nullptr);
  if (attach == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  attach->setBoneName("Hand");
  attach->setChildObjectId(k_invalid_object_id);

  attach->apply(*skeleton);
  expect_true("invalid child reports skip status",
              attach->getLastApplyStatus() ==
                  SkeletonAttachApplyStatus::SkippedInvalidChild);
  expect_true("child position unchanged",
              vec3_near(child->getPosition(), position_before));
  expect_true("child rotation unchanged",
              quat_near(child->getRotation(), rotation_before));
  expect_true("child scale unchanged",
              vec3_near(child->getScale(), scale_before));

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 3.2: destroyed child ObjectId skips transform write without crash.
void test_destroyed_child_skips_transform_write() {
  using namespace Blunder;

  ClassDB::initialize();

  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  expect_true("host created", host != nullptr);
  expect_true("child created", child != nullptr);
  if (host == nullptr || child == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  child->setParent(host);
  child->setPosition(Vec3(1.0f, 2.0f, 3.0f));
  child->setRotation(glm::angleAxis(0.5f, Vec3(0.0f, 1.0f, 0.0f)));
  child->setScale(Vec3(2.0f, 2.0f, 2.0f));

  Skeleton* skeleton = host->ensureSkeleton();
  const int hand = skeleton->addBone("Hand", -1);
  skeleton->resetPoseToRest();

  SkeletonAttachModifier* attach = host->addSkeletonAttachModifier();
  expect_true("attach modifier created", attach != nullptr);
  if (attach == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  attach->setBoneName("Hand");
  attach->setChildObjectId(child_id);
  ObjectDB::destroy(child_id);

  attach->apply(*skeleton);
  expect_true("destroyed child reports skip status",
              attach->getLastApplyStatus() ==
                  SkeletonAttachApplyStatus::SkippedChildNotFound);

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 3.2: invalid bone name skips transform write without crash.
void test_invalid_bone_name_skips_transform_write() {
  using namespace Blunder;

  ClassDB::initialize();

  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  expect_true("host created", host != nullptr);
  expect_true("child created", child != nullptr);
  if (host == nullptr || child == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  child->setParent(host);
  child->setPosition(Vec3(1.0f, 2.0f, 3.0f));
  child->setRotation(glm::angleAxis(0.5f, Vec3(0.0f, 1.0f, 0.0f)));
  child->setScale(Vec3(2.0f, 2.0f, 2.0f));
  const Vec3 position_before = child->getPosition();
  const Quat rotation_before = child->getRotation();
  const Vec3 scale_before = child->getScale();

  Skeleton* skeleton = host->ensureSkeleton();
  skeleton->addBone("Hand", -1);
  skeleton->resetPoseToRest();

  SkeletonAttachModifier* attach = host->addSkeletonAttachModifier();
  expect_true("attach modifier created", attach != nullptr);
  if (attach == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  attach->setBoneName("MissingBone");
  attach->setChildObjectId(child_id);

  attach->apply(*skeleton);
  expect_true("invalid bone reports skip status",
              attach->getLastApplyStatus() ==
                  SkeletonAttachApplyStatus::SkippedInvalidBone);
  expect_true("child position unchanged",
              vec3_near(child->getPosition(), position_before));
  expect_true("child rotation unchanged",
              quat_near(child->getRotation(), rotation_before));
  expect_true("child scale unchanged",
              vec3_near(child->getScale(), scale_before));

  ObjectDB::clear();
  ClassDB::shutdown();
}

}  // namespace

int main() {
  test_attach_modifier_copies_bone_world_to_child_transform();
  test_attach_modifier_after_sample();
  test_invalid_child_id_skips_transform_write();
  test_destroyed_child_skips_transform_write();
  test_invalid_bone_name_skips_transform_write();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_skeleton_attach_test: all passed\n");
  return 0;
}
