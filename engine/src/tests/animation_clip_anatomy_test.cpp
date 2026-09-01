#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_clip_anatomy.h"
#include "runtime/function/editor/animation_preview_controller.h"

#include <cstdio>

namespace {

int g_failures = 0;

constexpr const char* kIdleGuid = "11111111-1111-1111-1111-111111111111";
constexpr const char* kAttackGuid = "22222222-2222-2222-2222-222222222222";

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

Blunder::AnimationTrack makeTrack(const char* bone,
                                  Blunder::AnimationChannel channel,
                                  std::initializer_list<float> key_times) {
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = channel;
  track.interpolation = Blunder::AnimationInterpolation::Linear;
  for (const float time : key_times) {
    Blunder::AnimationKeyframe key;
    key.time = time;
    key.value = {0.0f, 0.0f, 0.0f};
    track.keys.push_back(key);
  }
  return track;
}

/// Scale B, Position A, Rotation A, Position B — interleaved on purpose.
Blunder::AnimationClipData makeInterleavedClip() {
  using namespace Blunder;
  AnimationClipData clip;
  clip.name = "Idle";
  clip.duration = 1.0f;
  clip.tracks.push_back(
      makeTrack("empty_parent-R", AnimationChannel::Scale, {0.0f, 1.0f}));
  clip.tracks.push_back(
      makeTrack("empty_parent-L", AnimationChannel::Translation, {0.0f}));
  clip.tracks.push_back(
      makeTrack("empty_parent-L", AnimationChannel::Rotation, {0.0f, 0.5f}));
  clip.tracks.push_back(
      makeTrack("empty_parent-R", AnimationChannel::Translation, {0.0f, 0.5f}));
  return clip;
}

void test_first_appearance_grouping_and_trs_order() {
  using namespace Blunder;

  const AnimationClipAnatomy anatomy = buildClipAnatomy(makeInterleavedClip());

  expect_true("clip name kept", anatomy.clip_name == "Idle");
  expect_true("duration kept", anatomy.duration == 1.0f);
  expect_true("two bone groups", anatomy.groups.size() == 2);
  if (anatomy.groups.size() != 2) {
    return;
  }

  expect_true("first appearance wins", anatomy.groups[0].bone == "empty_parent-R");
  expect_true("second group follows", anatomy.groups[1].bone == "empty_parent-L");

  const AnimationAnatomyGroup& right = anatomy.groups[0];
  expect_true("R has two channels", right.channels.size() == 2);
  if (right.channels.size() == 2) {
    expect_true("R Position before Scale",
                right.channels[0].channel == AnimationAnatomyChannel::Position &&
                    right.channels[1].channel == AnimationAnatomyChannel::Scale);
    expect_true("R keeps Position key times",
                right.channels[0].key_times.size() == 2 &&
                    right.channels[0].key_times[0] == 0.0f &&
                    right.channels[0].key_times[1] == 0.5f);
  }

  const AnimationAnatomyGroup& left = anatomy.groups[1];
  expect_true("L has two channels", left.channels.size() == 2);
  if (left.channels.size() == 2) {
    expect_true("L Position before Rotation",
                left.channels[0].channel == AnimationAnatomyChannel::Position &&
                    left.channels[1].channel == AnimationAnatomyChannel::Rotation);
  }

  expect_true("Position label", eastl::string(animationAnatomyChannelLabel(
                                    AnimationAnatomyChannel::Position)) ==
                                    "Position");
  expect_true("Rotation label", eastl::string(animationAnatomyChannelLabel(
                                    AnimationAnatomyChannel::Rotation)) ==
                                    "Rotation");
  expect_true("Scale label", eastl::string(animationAnatomyChannelLabel(
                                 AnimationAnatomyChannel::Scale)) == "Scale");
}

void test_missing_channels_are_omitted() {
  using namespace Blunder;

  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(
      makeTrack("empty_parent-R", AnimationChannel::Translation, {0.0f, 0.5f}));
  clip.tracks.push_back(
      makeTrack("empty_parent-R", AnimationChannel::Rotation, {0.0f}));

  const AnimationClipAnatomy anatomy = buildClipAnatomy(clip);
  expect_true("one group", anatomy.groups.size() == 1);
  if (anatomy.groups.empty()) {
    return;
  }
  expect_true("no synthesized Scale row", anatomy.groups[0].channels.size() == 2);
  for (const AnimationAnatomyChannelRow& row : anatomy.groups[0].channels) {
    expect_true("Scale omitted", row.channel != AnimationAnatomyChannel::Scale);
  }
}

void test_method_keys_do_not_create_rows() {
  using namespace Blunder;

  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(
      makeTrack("empty_parent-R", AnimationChannel::Translation, {0.0f}));
  AnimationMethodKey method;
  method.name = "FootStep";
  method.time = 0.25f;
  clip.method_keys.push_back(method);

  const AnimationClipAnatomy anatomy = buildClipAnatomy(clip);
  expect_true("method key adds no group", anatomy.groups.size() == 1);

  AnimationAnatomySession session;
  const eastl::vector<AnimationAnatomyRow> rows = session.buildRows(anatomy);
  expect_true("method key adds no row", rows.size() == 2);
  if (rows.size() == 2) {
    expect_true("group title first", rows[0].group && rows[0].label ==
                                                          "empty_parent-R");
    expect_true("channel row second",
                !rows[1].group && rows[1].label == "Position");
  }
}

void test_extra_clip_bone_is_listed() {
  using namespace Blunder;

  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(
      makeTrack("Hips", AnimationChannel::Translation, {0.0f}));
  clip.tracks.push_back(
      makeTrack("helper_R", AnimationChannel::Rotation, {0.0f, 0.75f}));

  // No Skeleton is supplied: the builder must never consult one.
  const AnimationClipAnatomy anatomy = buildClipAnatomy(clip);
  expect_true("two groups", anatomy.groups.size() == 2);
  if (anatomy.groups.size() != 2) {
    return;
  }
  expect_true("unmatched bone still listed", anatomy.groups[1].bone == "helper_R");
  expect_true("unmatched bone keeps its keys",
              anatomy.groups[1].channels.size() == 1 &&
                  anatomy.groups[1].channels[0].key_times.size() == 2);
}

void test_filter_hides_unmatched_groups() {
  using namespace Blunder;

  const AnimationClipAnatomy anatomy = buildClipAnatomy(makeInterleavedClip());

  AnimationAnatomySession session;
  expect_true("no filter shows every group",
              session.buildRows(anatomy).size() == 6);

  session.setFilter("parent-R");
  const eastl::vector<AnimationAnatomyRow> filtered = session.buildRows(anatomy);
  expect_true("filter keeps one group", filtered.size() == 3);
  for (const AnimationAnatomyRow& row : filtered) {
    expect_true("unmatched group hidden", row.bone == "empty_parent-R");
  }

  session.setFilter("Position");
  expect_true("channel words are not the filter target",
              session.buildRows(anatomy).empty());

  session.setFilter("");
  expect_true("cleared filter restores groups",
              session.buildRows(anatomy).size() == 6);
}

void test_fold_is_session_only_and_resets_on_clip_change() {
  using namespace Blunder;

  const AnimationClipAnatomy anatomy = buildClipAnatomy(makeInterleavedClip());

  AnimationAnatomySession session;
  expect_true("clip bind clears fold", session.syncRulerClipName("Idle"));
  expect_true("groups default expanded", session.buildRows(anatomy).size() == 6);

  session.toggleCollapsed("empty_parent-R");
  expect_true("group is collapsed", session.isCollapsed("empty_parent-R"));
  const eastl::vector<AnimationAnatomyRow> folded = session.buildRows(anatomy);
  expect_true("collapsed group hides channels", folded.size() == 4);
  expect_true("collapsed group keeps its place",
              folded[0].group && folded[0].bone == "empty_parent-R" &&
                  folded[0].collapsed);

  expect_true("same clip name keeps fold", !session.syncRulerClipName("Idle"));
  expect_true("fold survives a same-clip sync",
              session.isCollapsed("empty_parent-R"));

  expect_true("ruler clip change clears fold",
              session.syncRulerClipName("Attack"));
  expect_true("group re-expanded", !session.isCollapsed("empty_parent-R"));
  expect_true("every group re-expands", session.buildRows(anatomy).size() == 6);
}

Blunder::Object* makeAnatomyTreeObject() {
  using namespace Blunder;

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  if (object == nullptr) {
    return nullptr;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  if (skeleton == nullptr || player == nullptr || tree == nullptr) {
    return nullptr;
  }
  // Deliberately unrelated to the clip's bones: anatomy never consults Skeleton.
  skeleton->addBone("Hips", -1);

  AnimationClipData idle = makeInterleavedClip();
  AnimationClipData attack;
  attack.name = "Attack";
  attack.duration = 0.6f;
  attack.tracks.push_back(
      makeTrack("helper_R", AnimationChannel::Translation, {0.0f, 0.6f}));

  player->setClipGuid("idle", kIdleGuid);
  player->injectClipData(kIdleGuid, idle);
  player->setClipGuid("attack", kAttackGuid);
  player->injectClipData(kAttackGuid, attack);
  return object;
}

void test_controller_resolves_ruler_clip_anatomy() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makeAnatomyTreeObject();
  expect_true("object created", object != nullptr);
  AnimationTree* tree = object->getAnimationTree();
  tree->setActive(true);

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  expect_true("window bound", controller.windowBound());
  controller.play();

  controller.refreshClipAnatomy();
  expect_true("ruler clip resolved", controller.clipAnatomy().groups.size() == 2);
  expect_true("anatomy rows built", controller.anatomyRows().size() == 6);

  controller.toggleAnatomyGroup("empty_parent-R");
  expect_true("fold applied to rows", controller.anatomyRows().size() == 4);

  controller.setAnatomyFilter("parent-L");
  expect_true("filter applied to rows", controller.anatomyRows().size() == 3);
  controller.setAnatomyFilter("");

  // Fire-style insert swaps the ruler clip: anatomy follows and groups re-expand.
  expect_true("oneshot requested", controller.requestOneShot("attack"));
  controller.refreshClipAnatomy();
  expect_true("anatomy switched clip",
              controller.clipAnatomy().groups.size() == 1 &&
                  controller.clipAnatomy().groups[0].bone == "helper_R");
  expect_true("ruler clip change re-expanded groups",
              !controller.isAnatomyGroupCollapsed("empty_parent-R"));
  expect_true("switched clip rows", controller.anatomyRows().size() == 2);

  controller.clearTarget();
  controller.refreshClipAnatomy();
  expect_true("unbound shows no anatomy", controller.anatomyRows().empty());

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_unbound_selection_has_no_anatomy() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("plain object created", object != nullptr);

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  expect_true("no tree means unbound", !controller.windowBound());

  controller.refreshClipAnatomy();
  expect_true("no anatomy without a tree", controller.clipAnatomy().groups.empty());
  expect_true("no rows without a tree", controller.anatomyRows().empty());

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

}  // namespace

int main() {
  test_first_appearance_grouping_and_trs_order();
  test_missing_channels_are_omitted();
  test_method_keys_do_not_create_rows();
  test_extra_clip_bone_is_listed();
  test_filter_hides_unmatched_groups();
  test_fold_is_session_only_and_resets_on_clip_change();
  test_controller_resolves_ruler_clip_anatomy();
  test_unbound_selection_has_no_anatomy();

  Blunder::ObjectDB::clear();
  Blunder::LifecycleDispatch::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("animation_clip_anatomy_test: all passed\n");
  std::fflush(stdout);
  return 0;
}
