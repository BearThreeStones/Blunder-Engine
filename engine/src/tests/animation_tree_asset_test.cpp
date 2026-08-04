#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/animation_tree_asset.h"
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

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = 1e-3f) {
  return float_near(a.x, b.x, eps) && float_near(a.y, b.y, eps) &&
         float_near(a.z, b.z, eps);
}

Blunder::AnimationTrack makeTranslationTrack(
    const char* bone, const Blunder::Vec3& translation, float duration) {
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = Blunder::AnimationChannel::Translation;
  track.interpolation = Blunder::AnimationInterpolation::Constant;
  Blunder::AnimationKeyframe a;
  a.time = 0.0f;
  a.value = {translation.x, translation.y, translation.z};
  Blunder::AnimationKeyframe b = a;
  b.time = duration;
  track.keys.push_back(a);
  track.keys.push_back(b);
  return track;
}

Blunder::AnimationClipData makeConstClip(const Blunder::Vec3& translation) {
  Blunder::AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack("Hips", translation, 1.0f));
  return clip;
}

Blunder::AnimationTreeTopologyData makeLocomotionTopology() {
  Blunder::AnimationTreeTopologyData topology;
  Blunder::AnimationTreeTopologyData::BlendSpace1DDef space;
  space.node_name = "Locomotion";
  space.scalar = 0.0f;
  space.points.push_back({"idle", 0.0f});
  space.points.push_back({"walk", 1.0f});
  topology.blend_spaces_1d.push_back(eastl::move(space));
  topology.base_blend_space_node = "Locomotion";
  Blunder::AnimationTreeTopologyData::StateDef state;
  state.name = "Move";
  state.kind = "blendSpace1D";
  state.blend_space_node = "Locomotion";
  topology.states.push_back(eastl::move(state));
  return topology;
}

struct Fixture {
  Blunder::Skeleton skeleton;
  Blunder::AnimationPlayer player;
  Blunder::AnimationTree tree;

  Fixture() {
    skeleton.addBone("Hips", -1);
    player.bindSamplingSkeleton(&skeleton);
    tree.bindAnimationPlayer(&player);
    tree.bindSamplingSkeleton(&skeleton);
    player.setClipGuid("idle", "11111111-1111-1111-1111-111111111111");
    player.setClipGuid("walk", "22222222-2222-2222-2222-222222222222");
    player.injectClipData("11111111-1111-1111-1111-111111111111",
                          makeConstClip(Blunder::Vec3(0.0f, 0.0f, 0.0f)));
    player.injectClipData("22222222-2222-2222-2222-222222222222",
                          makeConstClip(Blunder::Vec3(10.0f, 0.0f, 0.0f)));
  }
};

void test_animation_tree_asset_yaml_round_trip() {
  using namespace Blunder;
  const AnimationTreeTopologyData original = makeLocomotionTopology();
  const eastl::string yaml =
      AssetYaml::serializeAnimationTreeTopologyData(original);
  AnimationTreeTopologyData parsed;
  expect_true("parse topology",
              AssetYaml::parseAnimationTreeTopologyData(yaml, parsed));
  expect_true("base node", parsed.base_blend_space_node == "Locomotion");
  expect_true("one space", parsed.blend_spaces_1d.size() == 1);
  expect_true("two points",
              parsed.blend_spaces_1d[0].points.size() == 2);
  expect_true("one state", parsed.states.size() == 1);
  expect_true("state kind", parsed.states[0].kind == "blendSpace1D");

  AnimationTreeAssetDescriptor descriptor;
  descriptor.guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  descriptor.source = "resources/AnimationTrees/dog.animtree.yaml";
  const eastl::string desc_yaml =
      AssetYaml::serializeAnimationTreeAssetDescriptor(descriptor);
  AnimationTreeAssetDescriptor parsed_desc;
  expect_true("parse descriptor",
              AssetYaml::parseAnimationTreeAssetDescriptor(desc_yaml,
                                                           parsed_desc));
  expect_true("guid", parsed_desc.guid == descriptor.guid);
  expect_true("source", parsed_desc.source == descriptor.source);
}

void test_tree_uses_asset_topology_as_base() {
  using namespace Blunder;
  Fixture f;
  const AnimationTreeTopologyData topology = makeLocomotionTopology();
  f.tree.setAssetGuid("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  expect_true("apply asset topology",
              applyAnimationTreeTopologyData(f.tree, topology));
  expect_true("asset guid retained",
              f.tree.getAssetGuid() == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  expect_true("activate", f.tree.setActive(true));
  expect_true("asset base idle pose",
              vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                        Vec3(0.0f, 0.0f, 0.0f)));
}

void test_instance_overrides_on_asset_base() {
  using namespace Blunder;
  Fixture f;
  expect_true("apply asset",
              applyAnimationTreeTopologyData(f.tree, makeLocomotionTopology()));

  AnimationTreeInstanceOverrides overrides;
  overrides.blend_space_scalars.push_back({"Locomotion", 1.0f});
  overrides.has_active = true;
  overrides.active = true;
  applyAnimationTreeInstanceOverrides(f.tree, overrides);

  expect_true("override scalar walk pose",
              vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                        Vec3(10.0f, 0.0f, 0.0f)));
}

void test_no_asset_embed_topology_still_works() {
  using namespace Blunder;
  Fixture f;
  // Phase 4 embed path: no asset GUID, authored in place.
  expect_true("embed points",
              f.tree.addBlendSpacePoint("Locomotion", "idle", 0.0f));
  expect_true("embed walk",
              f.tree.addBlendSpacePoint("Locomotion", "walk", 1.0f));
  f.tree.setBlendSpaceScalar("Locomotion", 0.5f);
  expect_true("embed base", f.tree.setBaseBlendSpaceNode("Locomotion"));
  expect_true("no asset guid", f.tree.getAssetGuid().empty());
  expect_true("activate", f.tree.setActive(true));
  expect_true("embed midpoint",
              vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                        Vec3(5.0f, 0.0f, 0.0f)));
}

void test_inspector_authorship_via_topology_yaml_no_canvas() {
  using namespace Blunder;
  // Inspector Done path: author topology as YAML body (no visual canvas).
  Fixture f;
  AnimationTreeTopologyData authored = makeLocomotionTopology();
  authored.blend_spaces_1d[0].scalar = 1.0f;
  const eastl::string yaml =
      AssetYaml::serializeAnimationTreeTopologyData(authored);
  AnimationTreeTopologyData loaded;
  expect_true("inspector yaml load",
              AssetYaml::parseAnimationTreeTopologyData(yaml, loaded));
  expect_true("apply authored", applyAnimationTreeTopologyData(f.tree, loaded));
  expect_true("activate", f.tree.setActive(true));
  expect_true("inspector authored walk",
              vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                        Vec3(10.0f, 0.0f, 0.0f)));
}

}  // namespace

int main() {
  test_animation_tree_asset_yaml_round_trip();
  test_tree_uses_asset_topology_as_base();
  test_instance_overrides_on_asset_base();
  test_no_asset_embed_topology_still_works();
  test_inspector_authorship_via_topology_yaml_no_canvas();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
