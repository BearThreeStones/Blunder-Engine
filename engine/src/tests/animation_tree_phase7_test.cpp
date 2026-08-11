#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/resource/asset/asset_yaml.h"

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

bool float_near(float a, float b, float eps = 1e-5f) {
  return std::fabs(a - b) < eps;
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

void bindPoseClip(Blunder::AnimationPlayer& player, const char* clip_name,
                  const char* guid, float duration,
                  const Blunder::Vec3& translation) {
  using namespace Blunder;
  eastl::string guid_str(guid);
  AnimationClipData clip;
  clip.name = clip_name;
  clip.duration = duration;
  clip.tracks.push_back(makeTranslationTrack(
      "Bone", AnimationInterpolation::Constant,
      {{0.0f, translation}, {duration, translation}}));
  player.setClipGuid(clip_name, guid_str);
  player.injectClipData(guid_str, clip);
}

struct Phase7Rig {
  Blunder::Object* object{nullptr};
  Blunder::AnimationPlayer* player{nullptr};
  Blunder::AnimationTree* tree{nullptr};
  Blunder::Skeleton* skeleton{nullptr};

  void setupTwoStates() {
    using namespace Blunder;
    ObjectDB::clear();
    const ObjectId id = ObjectDB::create();
    object = ObjectDB::get(id);
    skeleton = object->ensureSkeleton();
    player = object->ensureAnimationPlayer();
    tree = object->ensureAnimationTree();

    skeleton->addBone("Bone", -1);
    skeleton->setBoneInverseBind(0, Mat4(1.0f));
    skeleton->resetPoseToRest();

    bindPoseClip(*player, "idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 1.0f,
                 Vec3(0.0f, 0.0f, 0.0f));
    bindPoseClip(*player, "walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 1.0f,
                 Vec3(0.0f, 2.0f, 0.0f));

    tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
    tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
    tree->setStateBlendSpace("IdleState", "Locomotion");
    tree->setStateClip("WalkState", "walk");
    expect_true("activate", tree->setActive(true));
    expect_true("start idle", tree->start("IdleState"));
  }

  void teardown() {
    using namespace Blunder;
    if (object != nullptr) {
      ObjectDB::destroy(object->getId());
    }
    ObjectDB::clear();
    object = nullptr;
    player = nullptr;
    tree = nullptr;
    skeleton = nullptr;
  }
};

void test_tree_param_bool_float_get_set() {
  Phase7Rig rig;
  rig.setupTwoStates();
  expect_true("rig", rig.tree != nullptr);
  if (rig.tree == nullptr) {
    return;
  }

  rig.tree->setTreeParamBool("wantTrip", true);
  expect_true("bool true", rig.tree->getTreeParamBool("wantTrip") == true);
  rig.tree->setTreeParamBool("wantTrip", false);
  expect_true("bool false", rig.tree->getTreeParamBool("wantTrip") == false);

  rig.tree->setTreeParamFloat("speedGate", 0.75f);
  expect_true("float set",
              float_near(rig.tree->getTreeParamFloat("speedGate"), 0.75f));
  expect_true("missing bool default",
              rig.tree->getTreeParamBool("missingBool") == false);
  expect_true("missing float default",
              float_near(rig.tree->getTreeParamFloat("missingFloat"), 0.0f));

  rig.teardown();
}

void test_transition_single_predicate_auto_travel() {
  Phase7Rig rig;
  rig.setupTwoStates();
  if (rig.tree == nullptr) {
    return;
  }

  Blunder::StateMachineTransition edge;
  edge.from_state = "IdleState";
  edge.to_state = "WalkState";
  edge.source = Blunder::TransitionConditionSource::TreeParam;
  edge.param_name = "goWalk";
  edge.is_bool_predicate = true;
  edge.bool_operand = true;
  edge.priority = 0;
  expect_true("add edge", rig.tree->addTransition(edge));

  expect_true("still idle",
              rig.tree->getCurrentStateName() == "IdleState");
  rig.tree->setTreeParamBool("goWalk", true);
  rig.tree->advance(0.016f);
  expect_true("auto travel walk",
              rig.tree->getCurrentStateName() == "WalkState");

  rig.teardown();
}

void test_transition_hybrid_drive_and_param() {
  Phase7Rig rig;
  rig.setupTwoStates();
  if (rig.tree == nullptr) {
    return;
  }

  Blunder::StateMachineTransition drive_edge;
  drive_edge.from_state = "IdleState";
  drive_edge.to_state = "WalkState";
  drive_edge.source = Blunder::TransitionConditionSource::BlendSpace1DScalar;
  drive_edge.param_name = "Locomotion";
  drive_edge.is_bool_predicate = false;
  drive_edge.op = Blunder::TransitionCompareOp::Ge;
  drive_edge.float_operand = 0.5f;
  drive_edge.priority = 1;
  expect_true("add drive edge", rig.tree->addTransition(drive_edge));

  rig.tree->setBlendSpaceScalar("Locomotion", 0.2f);
  rig.tree->advance(0.016f);
  expect_true("below threshold stays",
              rig.tree->getCurrentStateName() == "IdleState");

  rig.tree->setBlendSpaceScalar("Locomotion", 0.8f);
  rig.tree->advance(0.016f);
  expect_true("drive threshold travels",
              rig.tree->getCurrentStateName() == "WalkState");

  rig.teardown();
}

void test_transition_priority_multi_true() {
  Phase7Rig rig;
  rig.setupTwoStates();
  if (rig.tree == nullptr) {
    return;
  }

  expect_true("add trip state",
              rig.tree->setStateClip("TripState", "idle"));

  Blunder::StateMachineTransition low;
  low.from_state = "IdleState";
  low.to_state = "WalkState";
  low.source = Blunder::TransitionConditionSource::TreeParam;
  low.param_name = "flag";
  low.is_bool_predicate = true;
  low.bool_operand = true;
  low.priority = 1;
  expect_true("add low", rig.tree->addTransition(low));

  Blunder::StateMachineTransition high;
  high.from_state = "IdleState";
  high.to_state = "TripState";
  high.source = Blunder::TransitionConditionSource::TreeParam;
  high.param_name = "flag";
  high.is_bool_predicate = true;
  high.bool_operand = true;
  high.priority = 10;
  expect_true("add high", rig.tree->addTransition(high));

  rig.tree->setTreeParamBool("flag", true);
  rig.tree->advance(0.016f);
  expect_true("higher priority wins",
              rig.tree->getCurrentStateName() == "TripState");

  rig.teardown();
}

void test_travel_coexists_when_no_edge() {
  Phase7Rig rig;
  rig.setupTwoStates();
  if (rig.tree == nullptr) {
    return;
  }

  Blunder::StateMachineTransition edge;
  edge.from_state = "IdleState";
  edge.to_state = "WalkState";
  edge.source = Blunder::TransitionConditionSource::TreeParam;
  edge.param_name = "never";
  edge.is_bool_predicate = true;
  edge.bool_operand = true;
  edge.priority = 0;
  expect_true("add unused edge", rig.tree->addTransition(edge));

  expect_true("forced travel", rig.tree->travel("WalkState"));
  expect_true("walk after travel",
              rig.tree->getCurrentStateName() == "WalkState");
  expect_true("forced start back", rig.tree->start("IdleState"));
  expect_true("idle after start",
              rig.tree->getCurrentStateName() == "IdleState");

  rig.teardown();
}

void test_inactive_tree_does_not_auto_travel() {
  Phase7Rig rig;
  rig.setupTwoStates();
  if (rig.tree == nullptr) {
    return;
  }

  Blunder::StateMachineTransition edge;
  edge.from_state = "IdleState";
  edge.to_state = "WalkState";
  edge.source = Blunder::TransitionConditionSource::TreeParam;
  edge.param_name = "go";
  edge.is_bool_predicate = true;
  edge.bool_operand = true;
  edge.priority = 0;
  expect_true("add edge", rig.tree->addTransition(edge));
  expect_true("deactivate", rig.tree->setActive(false));
  rig.tree->setTreeParamBool("go", true);
  rig.tree->advance(0.016f);
  expect_true("no auto when inactive",
              rig.tree->getCurrentStateName() == "IdleState");

  rig.teardown();
}

void test_topology_asset_round_trip_transitions_params_layout() {
  using namespace Blunder;
  Phase7Rig rig;
  rig.setupTwoStates();
  if (rig.tree == nullptr) {
    return;
  }

  rig.tree->setTreeParamBool("goWalk", false);
  rig.tree->setTreeParamFloat("speedGate", 0.4f);
  StateMachineTransition edge;
  edge.from_state = "IdleState";
  edge.to_state = "WalkState";
  edge.source = TransitionConditionSource::TreeParam;
  edge.param_name = "goWalk";
  edge.is_bool_predicate = true;
  edge.bool_operand = true;
  edge.priority = 3;
  expect_true("add edge", rig.tree->addTransition(edge));
  rig.tree->setCanvasNodePosition("IdleState", 10.0f, 20.0f);
  rig.tree->setCanvasNodePosition("WalkState", 120.0f, 40.0f);

  AnimationTreeTopologyData exported;
  rig.tree->exportTopologyData(exported);
  expect_true("export params", exported.tree_params.size() == 2);
  expect_true("export transitions", exported.transitions.size() == 1);
  expect_true("export layout", exported.canvas_layout.size() == 2);

  const eastl::string yaml =
      AssetYaml::serializeAnimationTreeTopologyData(exported);
  AnimationTreeTopologyData parsed;
  expect_true("parse yaml",
              AssetYaml::parseAnimationTreeTopologyData(yaml, parsed));
  expect_true("parsed transitions", parsed.transitions.size() == 1);
  expect_true("parsed priority", parsed.transitions[0].priority == 3);
  expect_true("parsed params", parsed.tree_params.size() == 2);
  expect_true("parsed layout", parsed.canvas_layout.size() == 2);

  AnimationTree restored;
  restored.bindAnimationPlayer(rig.player);
  restored.bindSamplingSkeleton(rig.skeleton);
  expect_true("apply topology", restored.applyTopologyData(parsed));
  expect_true("restored bool param",
              restored.getTreeParamBool("goWalk") == false);
  expect_true("restored float param",
              float_near(restored.getTreeParamFloat("speedGate"), 0.4f));
  float x = 0.0f;
  float y = 0.0f;
  expect_true("layout idle",
              restored.getCanvasNodePosition("IdleState", x, y) &&
                  float_near(x, 10.0f) && float_near(y, 20.0f));

  expect_true("activate restored", restored.setActive(true));
  expect_true("start idle restored", restored.start("IdleState"));
  restored.setTreeParamBool("goWalk", true);
  restored.advance(0.016f);
  expect_true("restored edge fires",
              restored.getCurrentStateName() == "WalkState");

  rig.teardown();
}

}  // namespace

int main() {
  test_tree_param_bool_float_get_set();
  test_transition_single_predicate_auto_travel();
  test_transition_hybrid_drive_and_param();
  test_transition_priority_multi_true();
  test_travel_coexists_when_no_edge();
  test_inactive_tree_does_not_auto_travel();
  test_topology_asset_round_trip_transitions_params_layout();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
