#include "runtime/platform/input/gameplay_input.h"

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
bool near1(float v) { return std::fabs(v - 1.f) < 1e-4f; }
bool near0(float v) { return std::fabs(v) < 1e-4f; }
}  // namespace

int main() {
  using namespace Blunder;
  GameplayInputState state;

  GameplayInputKeys base{};
  base.player_host = true;
  base.focused = true;
  base.paused = false;

  // Idle defaults
  {
    auto snap = state.sample(base);
    expect_true("idle move x", near0(snap.move_x));
    expect_true("idle move y", near0(snap.move_y));
    expect_true("idle jump", !snap.jump_pressed);
  }

  // W → +Y
  {
    state.reset();
    auto k = base;
    k.w = true;
    auto snap = state.sample(k);
    expect_true("w +y", near1(snap.move_y) && near0(snap.move_x));
  }

  // D → +X
  {
    state.reset();
    auto k = base;
    k.d = true;
    auto snap = state.sample(k);
    expect_true("d +x", near1(snap.move_x) && near0(snap.move_y));
  }

  // A+D cancel X
  {
    state.reset();
    auto k = base;
    k.a = true;
    k.d = true;
    auto snap = state.sample(k);
    expect_true("ad cancel", near0(snap.move_x) && near0(snap.move_y));
  }

  // W+D diagonal normalize
  {
    state.reset();
    auto k = base;
    k.w = true;
    k.d = true;
    auto snap = state.sample(k);
    const float len =
        std::sqrt(snap.move_x * snap.move_x + snap.move_y * snap.move_y);
    expect_true("diag len~1", near1(len));
  }

  // Jump edge shared across current()
  {
    state.reset();
    auto k = base;
    k.space = true;
    auto snap1 = state.sample(k);
    expect_true("jump edge1", snap1.jump_pressed);
    expect_true("jump current same", state.current().jump_pressed);
    auto snap2 = state.sample(k);  // still held
    expect_true("jump held not edge", !snap2.jump_pressed);
  }

  // Non-player idle
  {
    state.reset();
    auto k = base;
    k.player_host = false;
    k.w = true;
    k.space = true;
    auto snap = state.sample(k);
    expect_true("nonplayer idle move", near0(snap.move_x) && near0(snap.move_y));
    expect_true("nonplayer idle jump", !snap.jump_pressed);
  }

  // Unfocused idle
  {
    state.reset();
    auto k = base;
    k.focused = false;
    k.w = true;
    k.space = true;
    auto snap = state.sample(k);
    expect_true("unfocus idle", near0(snap.move_y) && !snap.jump_pressed);
  }

  // Pause discards jump; resume no buffered edge
  {
    state.reset();
    auto k = base;
    k.paused = true;
    k.space = true;
    expect_true("pause no jump", !state.sample(k).jump_pressed);
    k.space = false;
    state.sample(k);
    k.paused = false;
    k.space = false;
    expect_true("resume no buffer", !state.sample(k).jump_pressed);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("gameplay_input_test: OK\n");
  return 0;
}
