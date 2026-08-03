#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

void test_create_returns_valid_id() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  const SyncGroupId group = service.create();
  expect_true("create non-zero", group != k_invalid_sync_group_id);
  expect_true("empty group", service.getMemberCount(group) == 0);
}

void test_join_and_leave_members() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  const SyncGroupId group = service.create();

  expect_true("join player_a", service.join(group, &player_a));
  expect_true("member count one", service.getMemberCount(group) == 1);
  expect_true("player_a is member", service.isMember(group, &player_a));

  expect_true("join player_b", service.join(group, &player_b));
  expect_true("member count two", service.getMemberCount(group) == 2);
  expect_true("player_b is member", service.isMember(group, &player_b));

  expect_true("leave player_a", service.leave(group, &player_a));
  expect_true("member count after leave", service.getMemberCount(group) == 1);
  expect_true("player_a not member", !service.isMember(group, &player_a));
  expect_true("player_b still member", service.isMember(group, &player_b));
}

void test_destroy_releases_group() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId group = service.create();
  expect_true("join before destroy", service.join(group, &player));

  expect_true("destroy succeeds", service.destroy(group));
  expect_true("destroyed group has zero members",
              service.getMemberCount(group) == 0);
  expect_true("join fails after destroy", !service.join(group, &player));
  expect_true("leave fails after destroy", !service.leave(group, &player));
  expect_true("not member after destroy", !service.isMember(group, &player));
  expect_true("destroy again fails", !service.destroy(group));
}

void test_join_leave_validation() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId group = service.create();

  expect_true("null player join fails", !service.join(group, nullptr));
  expect_true("null player leave fails", !service.leave(group, nullptr));
  expect_true("invalid group join fails",
              !service.join(k_invalid_sync_group_id, &player));
  expect_true("invalid group leave fails",
              !service.leave(k_invalid_sync_group_id, &player));
  expect_true("leave non-member fails", !service.leave(group, &player));
  expect_true("duplicate join fails",
              service.join(group, &player) && !service.join(group, &player));
}

void test_player_may_join_multiple_groups() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId group_a = service.create();
  const SyncGroupId group_b = service.create();

  expect_true("join group_a", service.join(group_a, &player));
  expect_true("join group_b", service.join(group_b, &player));
  expect_true("member of group_a", service.isMember(group_a, &player));
  expect_true("member of group_b", service.isMember(group_b, &player));

  expect_true("leave group_a only", service.leave(group_a, &player));
  expect_true("not in group_a", !service.isMember(group_a, &player));
  expect_true("still in group_b", service.isMember(group_b, &player));
}

void test_destroy_invalid_and_unknown_ids() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  expect_true("destroy invalid id fails",
              !service.destroy(k_invalid_sync_group_id));

  const SyncGroupId live = service.create();
  expect_true("destroy unknown id fails", !service.destroy(live + 1000));
  expect_true("live group still usable", service.getMemberCount(live) == 0);
}

void test_join_unknown_group_id() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId live = service.create();

  expect_true("join unknown id fails", !service.join(live + 1000, &player));
  expect_true("leave unknown id fails", !service.leave(live + 1000, &player));
  expect_true("not member of unknown id",
              !service.isMember(live + 1000, &player));
  expect_true("unknown id member count zero",
              service.getMemberCount(live + 1000) == 0);
  expect_true("getMemberAt unknown id null",
              service.getMemberAt(live + 1000, 0) == nullptr);
}

void test_create_returns_distinct_ids() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  const SyncGroupId group_a = service.create();
  const SyncGroupId group_b = service.create();
  const SyncGroupId group_c = service.create();

  expect_true("ids non-zero",
              group_a != k_invalid_sync_group_id &&
                  group_b != k_invalid_sync_group_id &&
                  group_c != k_invalid_sync_group_id);
  expect_true("ids distinct",
              group_a != group_b && group_b != group_c && group_a != group_c);
}

void test_get_member_at_stable_insertion_order() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  AnimationPlayer player_c;
  const SyncGroupId group = service.create();

  expect_true("join order a", service.join(group, &player_a));
  expect_true("join order b", service.join(group, &player_b));
  expect_true("join order c", service.join(group, &player_c));
  expect_true("member count three", service.getMemberCount(group) == 3);

  expect_true("index 0 is player_a",
              service.getMemberAt(group, 0) == &player_a);
  expect_true("index 1 is player_b",
              service.getMemberAt(group, 1) == &player_b);
  expect_true("index 2 is player_c",
              service.getMemberAt(group, 2) == &player_c);
  expect_true("out of range null", service.getMemberAt(group, 3) == nullptr);
}

int main() {
  test_create_returns_valid_id();
  test_join_and_leave_members();
  test_destroy_releases_group();
  test_join_leave_validation();
  test_player_may_join_multiple_groups();
  test_destroy_invalid_and_unknown_ids();
  test_join_unknown_group_id();
  test_create_returns_distinct_ids();
  test_get_member_at_stable_insertion_order();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
