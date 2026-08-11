#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/animation_tree_asset.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/editor/animation_preview_controller.h"
#include "runtime/function/editor/animation_tree_canvas_document.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

namespace fs = std::filesystem;

void writeTextFile(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << text;
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
                  const char* guid, const Blunder::Vec3& translation) {
  Blunder::AnimationClipData clip;
  clip.name = clip_name;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Bone", Blunder::AnimationInterpolation::Constant,
      {{0.0f, translation}, {1.0f, translation}}));
  player.setClipGuid(clip_name, guid);
  player.injectClipData(guid, clip);
}

void test_canvas_asset_round_trip_edges_and_layout() {
  using namespace Blunder;
  ensureLogger();
  const fs::path root =
      fs::temp_directory_path() /
      ("blunder_phase7_eng_" +
       std::to_string(static_cast<long long>(
           fs::file_time_type::clock::now().time_since_epoch().count())));
  fs::create_directories(root / "Assets" / "AnimationTrees");
  fs::create_directories(root / "Resources" / "AnimationTrees");

  FileSystem file_system;
  FileSystemInitInfo info;
  info.project_root = root;
  file_system.initialize(info);

  AnimationTreeAssetDescriptor descriptor;
  descriptor.guid = "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee";
  descriptor.source = "resources/AnimationTrees/gate.animtree.yaml";
  writeTextFile(
      root / "Assets" / "AnimationTrees" / "gate.animationtree.yaml",
      std::string(
          AssetYaml::serializeAnimationTreeAssetDescriptor(descriptor).c_str()));
  writeTextFile(
      root / "Resources" / "AnimationTrees" / "gate.animtree.yaml",
      std::string(AssetYaml::serializeAnimationTreeTopologyData(
                      AnimationTreeTopologyData{})
                      .c_str()));

  AnimationTreeCanvasDocument browser_doc;
  expect_true("browser open",
              browser_doc.openFromDescriptorPath(
                  file_system,
                  "assets/AnimationTrees/gate.animationtree.yaml"));
  expect_true("add states",
              browser_doc.addState("Idle", "clip", "idle") &&
                  browser_doc.addState("Walk", "clip", "walk"));
  expect_true("param", browser_doc.declareTreeParamBool("want_walk", false));
  AnimationTreeTopologyData::TransitionDef edge;
  edge.from_state = "Idle";
  edge.to_state = "Walk";
  edge.source = "treeParam";
  edge.param_name = "want_walk";
  edge.is_bool_predicate = true;
  edge.bool_operand = true;
  edge.priority = 5;
  expect_true("edge", browser_doc.addTransition(edge));
  expect_true("layout", browser_doc.setNodePosition("Idle", 10.0f, 20.0f));
  expect_true("save", browser_doc.save(file_system));

  // Dual open path: same GUID via Object Inspector path.
  AnimationTreeCanvasSession::instance().close();
  expect_true("inspector open guid",
              AnimationTreeCanvasSession::instance().openGuid(
                  file_system, descriptor.guid,
                  "assets/AnimationTrees/gate.animationtree.yaml"));
  const AnimationTreeTopologyData& loaded =
      AnimationTreeCanvasSession::instance().document().topology();
  expect_true("edge round-trip", loaded.transitions.size() == 1);
  expect_true("priority", loaded.transitions[0].priority == 5);
  expect_true("layout round-trip", !loaded.canvas_layout.empty());

  // Dual-track Inspector YAML replace.
  AnimationTreeTopologyData inspector = loaded;
  inspector.transitions[0].priority = 1;
  expect_true(
      "inspector dual-track",
      AnimationTreeCanvasSession::instance().document().replaceTopologyFromInspectorYaml(
          AssetYaml::serializeAnimationTreeTopologyData(inspector)));
  expect_true(
      "inspector priority applied",
      AnimationTreeCanvasSession::instance().document().topology().transitions[0].priority ==
          1);

  AnimationTreeCanvasSession::instance().close();
  file_system.shutdown();
  std::error_code ec;
  fs::remove_all(root, ec);
}

void test_condition_auto_travel_priority_and_travel_coexist() {
  using namespace Blunder;
  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  skeleton->addBone("Bone", -1);
  skeleton->setBoneInverseBind(0, Mat4(1.0f));
  skeleton->resetPoseToRest();
  bindPoseClip(*player, "idle", "11111111-1111-1111-1111-111111111111",
               Vec3(0.0f, 0.0f, 0.0f));
  bindPoseClip(*player, "walk", "22222222-2222-2222-2222-222222222222",
               Vec3(0.0f, 2.0f, 0.0f));
  bindPoseClip(*player, "run", "33333333-3333-3333-3333-333333333333",
               Vec3(0.0f, 4.0f, 0.0f));
  tree->setStateClip("Idle", "idle");
  tree->setStateClip("Walk", "walk");
  tree->setStateClip("Run", "run");
  expect_true("active", tree->setActive(true));
  expect_true("start Idle", tree->start("Idle"));

  StateMachineTransition low;
  low.from_state = "Idle";
  low.to_state = "Walk";
  low.source = TransitionConditionSource::TreeParam;
  low.param_name = "go";
  low.is_bool_predicate = true;
  low.bool_operand = true;
  low.priority = 1;
  StateMachineTransition high;
  high.from_state = "Idle";
  high.to_state = "Run";
  high.source = TransitionConditionSource::TreeParam;
  high.param_name = "go";
  high.is_bool_predicate = true;
  high.bool_operand = true;
  high.priority = 10;
  expect_true("add low", tree->addTransition(low));
  expect_true("add high", tree->addTransition(high));
  tree->setTreeParamBool("go", true);
  tree->advance(0.016f);
  expect_true("priority picks Run", tree->getCurrentStateName() == "Run");

  expect_true("Travel coexist", tree->travel("Idle"));
  expect_true("forced Idle", tree->getCurrentStateName() == "Idle");

  // Edit scrub path without Behaviour Tick.
  AnimationPreviewController preview;
  preview.bindObject(object);
  expect_true("preview travel", preview.travel("Walk"));
  expect_true("preview state", tree->getCurrentStateName() == "Walk");

  ObjectDB::destroy(id);
  ObjectDB::clear();
}

}  // namespace

int main() {
  test_canvas_asset_round_trip_edges_and_layout();
  test_condition_auto_travel_priority_and_travel_coexist();
  Blunder::g_runtime_global_context.m_logger_system.reset();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase7_engineering_gates_test OK\n");
  return 0;
}
