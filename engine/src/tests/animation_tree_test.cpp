#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/class_db.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

Blunder::AnimationClipData make_test_clip(const char* name, float duration) {
  Blunder::AnimationClipData clip;
  clip.name = name;
  clip.duration = duration;
  return clip;
}

void test_object_hosts_animation_tree_with_player_and_skeleton() {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  expect_true("no tree initially", !object->hasAnimationTree());
  expect_true("get tree null", object->getAnimationTree() == nullptr);

  AnimationTree* tree = object->ensureAnimationTree();
  expect_true("ensure returns tree", tree != nullptr);
  expect_true("has tree", object->hasAnimationTree());
  expect_true("get matches ensure", object->getAnimationTree() == tree);

  AnimationTree* again = object->ensureAnimationTree();
  expect_true("ensure idempotent", again == tree);

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  expect_true("co-located skeleton", skeleton != nullptr);
  expect_true("co-located player", player != nullptr);
  expect_true("tree still same", object->getAnimationTree() == tree);

  object->clearAnimationTree();
  expect_true("cleared", !object->hasAnimationTree());
  expect_true("get null after clear", object->getAnimationTree() == nullptr);

  ObjectDB::clear();
}

void test_tree_resolves_clip_guid_via_player_map() {
  using namespace Blunder;

  AnimationPlayer player;
  AnimationTree tree;
  tree.bindAnimationPlayer(&player);

  const eastl::string walk_guid = "33333333-3333-3333-3333-333333333333";
  player.setClipGuid("LOOP-chocomel-walk", walk_guid);

  eastl::string resolved_guid;
  expect_true("resolve known clip name",
              tree.resolveClipGuid("LOOP-chocomel-walk", resolved_guid));
  expect_true("guid matches player map", resolved_guid == walk_guid);
  expect_true("unknown name fails",
              !tree.resolveClipGuid("missing-clip", resolved_guid));
}

void test_tree_resolves_clip_data_via_player_map() {
  using namespace Blunder;

  AnimationPlayer player;
  AnimationTree tree;
  tree.bindAnimationPlayer(&player);

  const eastl::string walk_guid = "33333333-3333-3333-3333-333333333333";
  player.setClipGuid("LOOP-chocomel-walk", walk_guid);
  player.injectClipData(walk_guid, make_test_clip("LOOP-chocomel-walk", 1.25f));

  AnimationClipData clip;
  expect_true("resolve clip for name",
              tree.resolveClipForName("LOOP-chocomel-walk", clip));
  expect_true("clip duration", clip.duration == 1.25f);
  expect_true("missing clip fails",
              !tree.resolveClipForName("missing-clip", clip));
}

void test_object_tree_resolves_through_hosted_player() {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  expect_true("tree bound to object player", tree != nullptr);

  const eastl::string walk_guid = "44444444-4444-4444-4444-444444444444";
  player->setClipGuid("LOOP-chocomel-walk", walk_guid);
  player->injectClipData(walk_guid, make_test_clip("LOOP-chocomel-walk", 2.0f));

  eastl::string resolved_guid;
  expect_true("object tree resolves guid",
              tree->resolveClipGuid("LOOP-chocomel-walk", resolved_guid));
  expect_true("guid from hosted player", resolved_guid == walk_guid);

  AnimationClipData clip;
  expect_true("object tree resolves clip",
              tree->resolveClipForName("LOOP-chocomel-walk", clip));
  expect_true("clip duration from hosted player", clip.duration == 2.0f);

  ObjectDB::clear();
}

void test_classdb_animation_tree_registration() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("AnimationTree registered", ClassDB::hasClass("AnimationTree"));

  AnimationPlayer player;
  AnimationTree tree;
  tree.bindAnimationPlayer(&player);

  const eastl::string walk_guid = "55555555-5555-5555-5555-555555555555";
  player.setClipGuid("LOOP-chocomel-walk", walk_guid);

  eastl::string resolved_guid;
  tree.resolveClipGuid("LOOP-chocomel-walk", resolved_guid);

  Variant has_player;
  expect_true("has_animation_player property",
              ClassDB::getProperty(&tree, "AnimationTree", "has_animation_player",
                                   has_player));
  expect_true("has_animation_player true", has_player.asBool());

  ClassDB::shutdown();
}

}  // namespace

int main() {
  test_object_hosts_animation_tree_with_player_and_skeleton();
  test_tree_resolves_clip_guid_via_player_map();
  test_tree_resolves_clip_data_via_player_map();
  test_object_tree_resolves_through_hosted_player();
  test_classdb_animation_tree_registration();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
