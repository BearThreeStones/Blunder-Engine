#include "runtime/function/editor/animation_tree_canvas_document.h"

#include "runtime/core/log/log_system.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/animation_tree_asset.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

struct TempProject {
  fs::path root;
  Blunder::FileSystem file_system;

  TempProject() {
    ensureLogger();
    root = fs::temp_directory_path() /
           ("blunder_animtree_canvas_" +
            std::to_string(
                static_cast<long long>(fs::file_time_type::clock::now()
                                           .time_since_epoch()
                                           .count())));
    fs::create_directories(root / "Assets" / "AnimationTrees");
    fs::create_directories(root / "Resources" / "AnimationTrees");
    Blunder::FileSystemInitInfo info;
    info.project_root = root;
    file_system.initialize(info);
  }

  ~TempProject() {
    file_system.shutdown();
    std::error_code ec;
    fs::remove_all(root, ec);
  }
};

Blunder::AnimationTreeTopologyData makeEmptyTopology() {
  Blunder::AnimationTreeTopologyData data;
  return data;
}

void test_canvas_authors_asset_topology_round_trip() {
  using namespace Blunder;
  TempProject project;

  AnimationTreeAssetDescriptor descriptor;
  descriptor.guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  descriptor.source = "resources/AnimationTrees/dog.animtree.yaml";
  writeTextFile(project.root / "Assets" / "AnimationTrees" / "dog.animationtree.yaml",
                std::string(AssetYaml::serializeAnimationTreeAssetDescriptor(
                                descriptor)
                                .c_str()));
  writeTextFile(project.root / "Resources" / "AnimationTrees" / "dog.animtree.yaml",
                std::string(AssetYaml::serializeAnimationTreeTopologyData(
                                makeEmptyTopology())
                                .c_str()));

  AnimationTreeCanvasDocument doc;
  expect_true(
      "open descriptor",
      doc.openFromDescriptorPath(
          project.file_system, "assets/AnimationTrees/dog.animationtree.yaml"));
  expect_true("guid", doc.assetGuid() == descriptor.guid);
  expect_true("add blend1d", doc.addBlendSpace1D("Locomotion"));
  expect_true("add idle", doc.addBlendSpace1DPoint("Locomotion", "idle", 0.0f));
  expect_true("add walk", doc.addBlendSpace1DPoint("Locomotion", "walk", 1.0f));
  expect_true("base", doc.setBaseBlendSpace1D("Locomotion"));
  expect_true("state",
              doc.addState("IdleState", "blendSpace1D", "Locomotion"));
  expect_true("oneshot", doc.setOneShotClip("trip"));
  expect_true("add2", doc.setAdd2Clip("turn"));
  expect_true("blend2d", doc.addBlendSpace2D("Pinda"));
  expect_true("blend2d point",
              doc.addBlendSpace2DPoint("Pinda", "walk", 0.0f, 0.0f));
  expect_true("layout", doc.setNodePosition("Locomotion", 120.0f, 40.0f));
  expect_true("dirty", doc.isDirty());
  expect_true("save", doc.save(project.file_system));
  expect_true("clean", !doc.isDirty());

  AnimationTreeCanvasDocument reopened;
  expect_true("reopen",
              reopened.openFromDescriptorPath(
                  project.file_system,
                  "assets/AnimationTrees/dog.animationtree.yaml"));
  expect_true("guid stable", reopened.assetGuid() == descriptor.guid);
  expect_true("blend spaces",
              reopened.topology().blend_spaces_1d.size() == 1);
  expect_true("points",
              reopened.topology().blend_spaces_1d[0].points.size() == 2);
  expect_true("states", reopened.topology().states.size() == 1);
  expect_true("oneshot clip", reopened.topology().oneshot_clip == "trip");
  expect_true("add2 clip", reopened.topology().add2_clip == "turn");
  expect_true("blend2d kept", reopened.topology().blend_spaces_2d.size() == 1);
  expect_true("layout kept",
              reopened.topology().canvas_layout.size() >= 1);
  float lx = 0.0f;
  float ly = 0.0f;
  bool found_layout = false;
  for (const auto& item : reopened.topology().canvas_layout) {
    if (item.node_id == "Locomotion") {
      lx = item.x;
      ly = item.y;
      found_layout = true;
    }
  }
  expect_true("layout node", found_layout);
  expect_true("layout x", lx == 120.0f);
  expect_true("layout y", ly == 40.0f);
}

void test_canvas_transition_edges_and_dual_track_inspector() {
  using namespace Blunder;
  AnimationTreeCanvasDocument doc;
  expect_true("open empty",
              doc.openFromYaml("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb",
                               "assets/AnimationTrees/x.animationtree.yaml",
                               "resources/AnimationTrees/x.animtree.yaml",
                               AssetYaml::serializeAnimationTreeTopologyData(
                                   makeEmptyTopology())));
  expect_true("state idle", doc.addState("Idle", "clip", "idle"));
  expect_true("state walk", doc.addState("Walk", "clip", "walk"));
  expect_true("param", doc.declareTreeParamBool("want_walk", false));

  AnimationTreeTopologyData::TransitionDef edge;
  edge.from_state = "Idle";
  edge.to_state = "Walk";
  edge.source = "treeParam";
  edge.param_name = "want_walk";
  edge.is_bool_predicate = true;
  edge.bool_operand = true;
  edge.priority = 10;
  expect_true("add edge", doc.addTransition(edge));

  const eastl::string yaml = doc.exportTopologyYaml();
  AnimationTreeTopologyData parsed;
  expect_true("parse export",
              AssetYaml::parseAnimationTreeTopologyData(yaml, parsed));
  expect_true("one edge", parsed.transitions.size() == 1);
  expect_true("edge priority", parsed.transitions[0].priority == 10);

  // Dual-track: Inspector edits same Asset topology YAML and canvas accepts it.
  parsed.transitions[0].priority = 3;
  const eastl::string inspector_yaml =
      AssetYaml::serializeAnimationTreeTopologyData(parsed);
  expect_true("inspector replace",
              doc.replaceTopologyFromInspectorYaml(inspector_yaml));
  expect_true("inspector priority",
              doc.topology().transitions[0].priority == 3);
}

void test_open_by_guid_same_asset() {
  using namespace Blunder;
  TempProject project;
  AnimationTreeAssetDescriptor descriptor;
  descriptor.guid = "cccccccc-cccc-cccc-cccc-cccccccccccc";
  descriptor.source = "resources/AnimationTrees/same.animtree.yaml";
  writeTextFile(
      project.root / "Assets" / "AnimationTrees" / "same.animationtree.yaml",
      std::string(
          AssetYaml::serializeAnimationTreeAssetDescriptor(descriptor).c_str()));
  writeTextFile(
      project.root / "Resources" / "AnimationTrees" / "same.animtree.yaml",
      std::string(AssetYaml::serializeAnimationTreeTopologyData(
                      makeEmptyTopology())
                      .c_str()));

  AnimationTreeCanvasSession& session = AnimationTreeCanvasSession::instance();
  session.close();
  expect_true("open by guid",
              session.openGuid(
                  project.file_system, descriptor.guid,
                  "assets/AnimationTrees/same.animationtree.yaml"));
  expect_true("session guid",
              session.document().assetGuid() == descriptor.guid);
  expect_true("path helper",
              isAnimationTreeAssetDescriptorPath(
                  "assets/AnimationTrees/same.animationtree.yaml"));
  expect_true("not mesh",
              !isAnimationTreeAssetDescriptorPath("assets/Meshes/a.mesh.yaml"));
  session.close();
}

void test_canvas_topology_usable_by_runtime() {
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

  AnimationClipData idle;
  idle.name = "idle";
  idle.duration = 1.0f;
  AnimationTrack track;
  track.bone = "Bone";
  track.channel = AnimationChannel::Translation;
  track.interpolation = AnimationInterpolation::Constant;
  AnimationKeyframe k0;
  k0.time = 0.0f;
  k0.value = {0.0f, 0.0f, 0.0f};
  track.keys.push_back(k0);
  idle.tracks.push_back(track);
  player->setClipGuid("idle", "11111111-1111-1111-1111-111111111111");
  player->injectClipData("11111111-1111-1111-1111-111111111111", idle);

  AnimationTreeCanvasDocument doc;
  expect_true("open",
              doc.openFromYaml("dddddddd-dddd-dddd-dddd-dddddddddddd",
                               "assets/x.animationtree.yaml",
                               "resources/x.animtree.yaml",
                               AssetYaml::serializeAnimationTreeTopologyData(
                                   makeEmptyTopology())));
  expect_true("bs", doc.addBlendSpace1D("Locomotion"));
  expect_true("pt", doc.addBlendSpace1DPoint("Locomotion", "idle", 0.0f));
  expect_true("base", doc.setBaseBlendSpace1D("Locomotion"));
  expect_true("apply",
              applyAnimationTreeTopologyData(*tree, doc.topology()));
  expect_true("active", tree->setActive(true));
  ObjectDB::destroy(id);
  ObjectDB::clear();
}

}  // namespace

int main() {
  test_canvas_transition_edges_and_dual_track_inspector();
  test_canvas_topology_usable_by_runtime();
  test_canvas_authors_asset_topology_round_trip();
  test_open_by_guid_same_asset();
  const int failures = g_failures;
  Blunder::g_runtime_global_context.m_logger_system.reset();
  if (failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("animation_tree_canvas_document_test OK\n");
  return 0;
}
