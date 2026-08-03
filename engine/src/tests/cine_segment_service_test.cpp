#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/cine_segment_service.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void bind_clip(Blunder::AnimationPlayer& player, const char* clip_name,
               const char* guid, float duration) {
  eastl::string guid_str(guid);
  player.setClipGuid(clip_name, guid_str);
  Blunder::AnimationClipData clip;
  clip.name = clip_name;
  clip.duration = duration;
  player.injectClipData(guid_str, clip);
}

}  // namespace

void test_initially_not_in_cine() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("not in cine initially", !service.isInCine());
}

void test_enter_sets_in_cine_mark() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter succeeds", service.enter());
  expect_true("in cine after enter", service.isInCine());
}

void test_end_clears_in_cine_mark() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter", service.enter());
  expect_true("end succeeds", service.end());
  expect_true("not in cine after end", !service.isInCine());
}

void test_end_when_not_in_cine_fails() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("end without enter fails", !service.end());
  expect_true("still not in cine", !service.isInCine());
}

void test_end_is_idempotent_after_first_end() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter", service.enter());
  expect_true("first end succeeds", service.end());
  expect_true("second end fails", !service.end());
  expect_true("not in cine", !service.isInCine());
}

void test_reenter_after_end() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter", service.enter());
  expect_true("end", service.end());
  expect_true("re-enter", service.enter());
  expect_true("in cine again", service.isInCine());
}

void test_enter_while_active_stays_in_cine() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("first enter", service.enter());
  expect_true("second enter", service.enter());
  expect_true("still in cine", service.isInCine());
  expect_true("end clears", service.end());
  expect_true("not in cine", !service.isInCine());
}

void test_finished_clip_does_not_auto_end_segment() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  AnimationPlayer player;
  bind_clip(player, "CINE-lead", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 0.05f);

  expect_true("enter cine", service.enter());
  expect_true("play lead clip", player.play("CINE-lead"));

  while (player.isPlaying()) {
    player.advance(0.02f);
  }

  expect_true("clip finished", !player.isPlaying());
  expect_true("segment still active without end", service.isInCine());

  expect_true("explicit end clears mark", service.end());
  expect_true("not in cine after end", !service.isInCine());
}

void test_enter_without_suppress_does_not_suppress_input() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter without suppress", service.enter(false));
  expect_true("in cine", service.isInCine());
  expect_true("input not suppressed", !service.isGameplayInputSuppressed());
}

void test_enter_with_suppress_sets_flag() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter with suppress", service.enter(true));
  expect_true("in cine", service.isInCine());
  expect_true("input suppressed", service.isGameplayInputSuppressed());
}

void test_end_restores_gameplay_input() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter with suppress", service.enter(true));
  expect_true("input suppressed while active", service.isGameplayInputSuppressed());
  expect_true("end", service.end());
  expect_true("input restored after end", !service.isGameplayInputSuppressed());
  expect_true("not in cine", !service.isInCine());
}

void test_suppress_flag_cleared_on_reset_for_tests() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter with suppress", service.enter(true));
  service.resetForTests();
  expect_true("not in cine after reset", !service.isInCine());
  expect_true("input not suppressed after reset", !service.isGameplayInputSuppressed());
}

void test_reenter_updates_suppress_flag() {
  using namespace Blunder;

  CineSegmentService& service = cineSegmentService();
  service.resetForTests();

  expect_true("enter without suppress", service.enter(false));
  expect_true("not suppressed", !service.isGameplayInputSuppressed());
  expect_true("re-enter with suppress", service.enter(true));
  expect_true("still in cine", service.isInCine());
  expect_true("now suppressed", service.isGameplayInputSuppressed());
}

int main() {
  test_initially_not_in_cine();
  test_enter_sets_in_cine_mark();
  test_end_clears_in_cine_mark();
  test_end_when_not_in_cine_fails();
  test_end_is_idempotent_after_first_end();
  test_reenter_after_end();
  test_enter_while_active_stays_in_cine();
  test_finished_clip_does_not_auto_end_segment();
  test_enter_without_suppress_does_not_suppress_input();
  test_enter_with_suppress_sets_flag();
  test_end_restores_gameplay_input();
  test_suppress_flag_cleared_on_reset_for_tests();
  test_reenter_updates_suppress_flag();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
