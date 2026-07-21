# Gameplay Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose Player-authoritative Gameplay Actions (Move 2D + Jump press-edge) to C# Behaviours via `Blunder.Api.Input` and new NativeAbi entry points.

**Architecture:** A pure native `GameplayInputState` turns keyboard/focus/pause/host flags into a per-frame Action snapshot. The Player host samples SDL once before Behaviour Tick. C-ABI getters read that snapshot; managed `Input` calls them through the existing registered `BlunderNativeAbi` table (ABI v3). Editor `InputSystem`/`GameCommand` stays untouched.

**Tech Stack:** C++20 `engine_runtime`, SDL3 keyboard/focus, C-ABI / NativeAbi, C# `net10.0` Blunder.Api, CMake `vs2026-debug`.

## Global Constraints

- Authoritative Gameplay Input only in **Player** host (`EngineHostMode::Player`); elsewhere idle
- Behaviours read **Actions**, not raw KeyCode / not `GameCommand`
- Slice Actions: **Move** (float x,y) + **Jump** (press-edge); edge **shared** for the sim frame
- Pause: **discard edges** (no buffered Jump on Resume); Move idle while paused
- Unfocused Player → idle
- Defaults: WASD + Space; diagonal **normalized**; Move `(x,y)` → world **+X / +Y** (Z-up)
- Extend **existing** NativeAbi; bump ABI version to **3**; no separate Input ABI
- Out of slice: remapping, gamepad, Edit Mode as input source, Pause edge buffering, DogWalk content
- Product bins: `bin/<Config>/`; tests under `engine/src/tests/`; preset `vs2026-debug` / `Debug`
- OpenSpec change: `openspec/changes/gameplay-input/` (proposal/design/specs/tasks already written)
- Glossary/ADR: `CONTEXT.md`, `docs/adr/0015-gameplay-input-nativeabi-actions.md`

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/platform/input/gameplay_input.h` | `GameplayInputKeys`, `GameplayInputSnapshot`, `GameplayInputState` |
| `engine/src/runtime/platform/input/gameplay_input.cpp` | Pure sample math + process-global state for C-ABI |
| `engine/src/runtime/core/reflection/engine_c_abi.h` | ABI v3; new getters; `BlunderNativeAbi` fields |
| `engine/src/runtime/core/reflection/engine_c_abi.cpp` | Getter impl + fill_from_process/module |
| `engine/src/runtime/engine.cpp` | Player: sample before Behaviour Tick |
| `engine/src/runtime/CMakeLists.txt` | Add `gameplay_input.cpp` if sources are listed explicitly |
| `engine/src/tests/gameplay_input_test.cpp` | Pure sampler TDD |
| `engine/src/tests/native_abi_test.cpp` | Completeness + version ≥ 3 |
| `engine/src/tests/CMakeLists.txt` | `gameplay_input_test` target |
| `engine/managed/Blunder.Api/NativeAbi.cs` | Two new function pointers |
| `engine/managed/Blunder.Api/Native.cs` | Completeness + wrappers |
| `engine/managed/Blunder.Api/Vec2.cs` | Move return type |
| `engine/managed/Blunder.Api/Input.cs` | Static façade |
| `engine/managed/Blunder.Api.NativeAbiTests/Program.cs` | Size 19; stub new entries |

**Do not modify for product Actions:** `input_system.h/.cpp` `GameCommand` semantics (editor/camera path).

```
SDL keys / focus / Pause / host
        │
        ▼
GameplayInputState::sample  ──► snapshot (move, jump edge)
        │
        ▼
C-ABI getters ◄── BlunderNativeAbi ◄── RegisterNativeAbi
        │
        ▼
Blunder.Api.Input  ◄── Behaviour.Tick
```

---

### Task 1: Pure GameplayInputState + native tests

**Files:**
- Create: `engine/src/runtime/platform/input/gameplay_input.h`
- Create: `engine/src/runtime/platform/input/gameplay_input.cpp`
- Create: `engine/src/tests/gameplay_input_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt` (add target after `native_abi_test` block)
- Modify: `engine/src/runtime/CMakeLists.txt` (add `platform/input/gameplay_input.cpp` next to `input_system.cpp`)

**Interfaces:**
- Consumes: none
- Produces:
  - `struct GameplayInputKeys { bool w,a,s,d,space; bool focused; bool paused; bool player_host; };`
  - `struct GameplayInputSnapshot { float move_x; float move_y; bool jump_pressed; };`
  - `class GameplayInputState { GameplayInputSnapshot sample(const GameplayInputKeys&); GameplayInputSnapshot current() const; void reset(); };`
  - Process helpers for later C-ABI: `GameplayInputState& gameplayInputState();` (or free functions wrapping a file-static state)

- [ ] **Step 1: Write the failing test**

Create `engine/src/tests/gameplay_input_test.cpp`:

```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build/vs2026-debug --config Debug --target gameplay_input_test
```

Expected: configure/build **FAIL** (missing sources / cannot open `gameplay_input.h`) — if the target is not wired yet, add the CMake target first pointing at the test file only; compile should still fail on missing header.

Wire a minimal target in `engine/src/tests/CMakeLists.txt` (mirror `native_abi_test` but link `engine_runtime` or whatever `input_system` tests use — prefer same libs as a small runtime unit test). Look at `play_pause_tick_gate_test` / similar for the link set; typical pattern:

```cmake
add_executable(gameplay_input_test
    "gameplay_input_test.cpp"
    "${CMAKE_SOURCE_DIR}/engine/src/runtime/platform/input/gameplay_input.cpp"
)
target_include_directories(gameplay_input_test PRIVATE
    "${CMAKE_SOURCE_DIR}/engine/src"
)
# link only what gameplay_input.cpp needs — keep it free of SDL for Task 1
add_test(NAME gameplay_input_test COMMAND gameplay_input_test)
```

If `gameplay_input.cpp` is not created yet, Step 2 fails as expected.

- [ ] **Step 3: Write minimal implementation**

`engine/src/runtime/platform/input/gameplay_input.h`:

```cpp
#pragma once

namespace Blunder {

struct GameplayInputKeys {
  bool w{false};
  bool a{false};
  bool s{false};
  bool d{false};
  bool space{false};
  bool focused{true};
  bool paused{false};
  bool player_host{false};
};

struct GameplayInputSnapshot {
  float move_x{0.f};
  float move_y{0.f};
  bool jump_pressed{false};
};

class GameplayInputState {
 public:
  GameplayInputSnapshot sample(const GameplayInputKeys& keys);
  GameplayInputSnapshot current() const { return m_current; }
  void reset();

 private:
  GameplayInputSnapshot m_current{};
  bool m_space_was_down{false};
};

GameplayInputState& gameplayInputState();

}  // namespace Blunder
```

`engine/src/runtime/platform/input/gameplay_input.cpp`:

```cpp
#include "runtime/platform/input/gameplay_input.h"

#include <cmath>

namespace Blunder {

void GameplayInputState::reset() {
  m_current = {};
  m_space_was_down = false;
}

GameplayInputSnapshot GameplayInputState::sample(const GameplayInputKeys& keys) {
  const bool authoritative =
      keys.player_host && keys.focused && !keys.paused;

  if (!authoritative) {
    m_space_was_down = keys.space;
    m_current = {};
    return m_current;
  }

  float x = 0.f;
  float y = 0.f;
  if (keys.d) {
    x += 1.f;
  }
  if (keys.a) {
    x -= 1.f;
  }
  if (keys.w) {
    y += 1.f;
  }
  if (keys.s) {
    y -= 1.f;
  }
  const float len = std::sqrt(x * x + y * y);
  if (len > 1.e-6f) {
    x /= len;
    y /= len;
  }

  const bool jump = keys.space && !m_space_was_down;
  m_space_was_down = keys.space;

  m_current.move_x = x;
  m_current.move_y = y;
  m_current.jump_pressed = jump;
  return m_current;
}

GameplayInputState& gameplayInputState() {
  static GameplayInputState s;
  return s;
}

}  // namespace Blunder
```

- [ ] **Step 4: Run test to verify it passes**

```powershell
cmake --build build/vs2026-debug --config Debug --target gameplay_input_test
.\build\vs2026-debug\engine\src\tests\Debug\gameplay_input_test.exe
```

Expected: `gameplay_input_test: OK`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/platform/input/gameplay_input.h engine/src/runtime/platform/input/gameplay_input.cpp engine/src/tests/gameplay_input_test.cpp engine/src/tests/CMakeLists.txt engine/src/runtime/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: add pure GameplayInputState sampler for Move/Jump actions

EOF
)"
```

---

### Task 2: C-ABI getters + NativeAbi v3

**Files:**
- Modify: `engine/src/runtime/core/reflection/engine_c_abi.h`
- Modify: `engine/src/runtime/core/reflection/engine_c_abi.cpp`
- Modify: `engine/src/tests/native_abi_test.cpp` (expect new pointers + version ≥ 3)

**Interfaces:**
- Consumes: `gameplayInputState().current()`
- Produces:
  - `#define BLUNDER_ENGINE_C_ABI_VERSION 3`
  - `int blunder_gameplay_input_get_move(float* out_x, float* out_y);`
  - `int blunder_gameplay_input_was_jump_pressed(int* out_pressed);`
  - `BlunderNativeAbi` fields: `gameplay_input_get_move`, `gameplay_input_was_jump_pressed`

- [ ] **Step 1: Write the failing assertions**

In `native_abi_test.cpp`, extend `expect_all_api_entries_non_null` with:

```cpp
  expect_true((std::string(label) + ": gameplay_input_get_move").c_str(),
              abi.gameplay_input_get_move != nullptr);
  expect_true((std::string(label) + ": gameplay_input_was_jump_pressed").c_str(),
              abi.gameplay_input_was_jump_pressed != nullptr);
```

And after fill_from_process, assert:

```cpp
  expect_true("abi version >= 3",
              process_abi.engine_abi_version() >= 3);
```

Also add a small functional check (same file or `gameplay_input_test` via C-ABI):

```cpp
  float mx = 1.f, my = 1.f;
  int jump = 1;
  gameplayInputState().reset();
  expect_true("cabi move ok",
              blunder_gameplay_input_get_move(&mx, &my) == BLUNDER_ENGINE_OK);
  expect_true("cabi move idle", mx == 0.f && my == 0.f);
  expect_true("cabi jump ok",
              blunder_gameplay_input_was_jump_pressed(&jump) == BLUNDER_ENGINE_OK);
  expect_true("cabi jump idle", jump == 0);
```

(Include `gameplay_input.h` if calling `gameplayInputState` from the test.)

- [ ] **Step 2: Run to verify fail**

```powershell
cmake --build build/vs2026-debug --config Debug --target native_abi_test
```

Expected: compile error (unknown fields / version still 2) or test FAIL on null pointers.

- [ ] **Step 3: Implement C-ABI + fill**

In `engine_c_abi.h`:

```cpp
#define BLUNDER_ENGINE_C_ABI_VERSION 3

// ... existing decls ...

BLUNDER_ENGINE_C_API int blunder_gameplay_input_get_move(float* out_x,
                                                         float* out_y);
BLUNDER_ENGINE_C_API int blunder_gameplay_input_was_jump_pressed(
    int* out_pressed);

typedef struct BlunderNativeAbi {
  // ... existing 17 fields ...
  int (*gameplay_input_get_move)(float* out_x, float* out_y);
  int (*gameplay_input_was_jump_pressed)(int* out_pressed);
} BlunderNativeAbi;
```

In `engine_c_abi.cpp` (include `gameplay_input.h`):

```cpp
int blunder_gameplay_input_get_move(float* out_x, float* out_y) {
  if (out_x == nullptr || out_y == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const GameplayInputSnapshot snap = gameplayInputState().current();
  *out_x = snap.move_x;
  *out_y = snap.move_y;
  return BLUNDER_ENGINE_OK;
}

int blunder_gameplay_input_was_jump_pressed(int* out_pressed) {
  if (out_pressed == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_pressed = gameplayInputState().current().jump_pressed ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}
```

Append to both fill functions:

```cpp
  out->gameplay_input_get_move = &blunder_gameplay_input_get_move;
  out->gameplay_input_was_jump_pressed =
      &blunder_gameplay_input_was_jump_pressed;
```

And module load:

```cpp
  BLUNDER_NATIVE_ABI_LOAD(gameplay_input_get_move,
                          "blunder_gameplay_input_get_move");
  BLUNDER_NATIVE_ABI_LOAD(gameplay_input_was_jump_pressed,
                          "blunder_gameplay_input_was_jump_pressed");
```

Ensure `gameplay_input.cpp` is linked into `blunder_engine_c` / `engine_runtime` (same as other C-ABI deps).

- [ ] **Step 4: Run tests**

```powershell
cmake --build build/vs2026-debug --config Debug --target native_abi_test
.\build\vs2026-debug\engine\src\tests\Debug\native_abi_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\gameplay_input_test.exe
```

Expected: both OK.

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/core/reflection/engine_c_abi.h engine/src/runtime/core/reflection/engine_c_abi.cpp engine/src/tests/native_abi_test.cpp
git commit -m "$(cat <<'EOF'
feat: expose Gameplay Input on C-ABI NativeAbi v3

EOF
)"
```

---

### Task 3: Player frame sampling before Behaviour Tick

**Files:**
- Modify: `engine/src/runtime/engine.cpp` (near existing `m_input_system->tick()` / Behaviour Tick block ~382–410)
- Optionally small helper in `gameplay_input.cpp`: `void sampleGameplayInputFromSdl(bool player_host, bool paused, bool focused, const bool* keyboard_state);`

**Interfaces:**
- Consumes: `GameplayInputState::sample`, `g_runtime_global_context.hostMode()`, `isPlayPaused()`, `m_window_system->getFocusMode()`, `SDL_GetKeyboardState`
- Produces: updated process snapshot before `dispatchObjectLifecycle`

- [ ] **Step 1: Write a focused regression in `gameplay_input_test` (already covers pause/focus) — add SDL-free host wiring note**

No new SDL unit test required if Task 1 covered keys. Add one integration-style comment test only if you can inject keys without SDL — otherwise rely on manual Player check in Step 4.

Optional: extract sampling helper and unit-test that `player_host=false` path is what Editor calls:

```cpp
void applyGameplayInputSample(const GameplayInputKeys& keys) {
  gameplayInputState().sample(keys);
}
```

- [ ] **Step 2: Confirm Behaviour Tick still gated on Pause**

Read `engine.cpp` — Pause already skips Tick via `args->paused`. Sampling MUST still run while paused so edge baseline syncs (call `sample` with `paused=true` even when Tick is skipped).

- [ ] **Step 3: Implement sampling in `tickOneFrame`**

After `m_input_system->tick();` (or immediately before Behaviour Tick), add:

```cpp
    {
      GameplayInputKeys keys{};
      keys.player_host =
          g_runtime_global_context.hostMode() == EngineHostMode::Player;
      keys.paused = g_runtime_global_context.isPlayPaused();
      keys.focused = false;
      if (g_runtime_global_context.m_window_system) {
        keys.focused =
            g_runtime_global_context.m_window_system->getFocusMode();
      }
      int key_count = 0;
      const bool* kb = SDL_GetKeyboardState(&key_count);
      if (kb != nullptr) {
        keys.w = kb[SDL_SCANCODE_W];
        keys.a = kb[SDL_SCANCODE_A];
        keys.s = kb[SDL_SCANCODE_S];
        keys.d = kb[SDL_SCANCODE_D];
        keys.space = kb[SDL_SCANCODE_SPACE];
      }
      gameplayInputState().sample(keys);
    }
```

Include `gameplay_input.h`, `engine_host_mode.h`, and SDL header as needed.

Do **not** change `InputSystem` GameCommand mapping.

- [ ] **Step 4: Build Player + smoke**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_player gameplay_input_test
.\build\vs2026-debug\engine\src\tests\Debug\gameplay_input_test.exe
```

Manual (optional): run `engine_player` with a project; confirm process stays alive. Full Behaviour Input check lands after Task 4.

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/engine.cpp
git commit -m "$(cat <<'EOF'
feat: sample Gameplay Input each Player frame before Tick

EOF
)"
```

---

### Task 4: Managed NativeAbi + Input façade

**Files:**
- Modify: `engine/managed/Blunder.Api/NativeAbi.cs`
- Modify: `engine/managed/Blunder.Api/Native.cs`
- Modify: `engine/managed/Blunder.Api.NativeAbiTests/Program.cs`
- Create: `engine/managed/Blunder.Api/Vec2.cs`
- Create: `engine/managed/Blunder.Api/Input.cs`

**Interfaces:**
- Consumes: NativeAbi `gameplay_input_get_move` / `gameplay_input_was_jump_pressed`
- Produces:
  - `public readonly struct Vec2 { float X, Y; }`
  - `public static class Input { public static Vec2 GetMove(); public static bool WasJumpPressed(); }`

- [ ] **Step 1: Update NativeAbiTests to expect 19 pointers (fail first)**

In `Program.cs`:

```csharp
Expect(
    sizeof(BlunderNativeAbi) == 19 * sizeof(nint),
    "BlunderNativeAbi layout size is 19 pointers");
```

Add stub fields before `Native.Register`:

```csharp
abi.gameplay_input_get_move = &StubGetMove;
abi.gameplay_input_was_jump_pressed = &StubWasJump;

// stubs:
[UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
static int StubGetMove(float* x, float* y)
{
    *x = 0.3f;
    *y = -0.4f;
    return Native.Ok;
}

[UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
static int StubWasJump(int* pressed)
{
    *pressed = 1;
    return Native.Ok;
}
```

After register:

```csharp
Vec2 move = Input.GetMove();
Expect(move.X == 0.3f && move.Y == -0.4f, "Input.GetMove via stub");
Expect(Input.WasJumpPressed(), "Input.WasJumpPressed via stub");
```

(If `Input` does not exist yet, test fails to compile — good.)

- [ ] **Step 2: Run managed test project to see failure**

```powershell
dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug
```

Expected: compile errors (missing fields / Input / size).

- [ ] **Step 3: Implement managed surface**

`NativeAbi.cs` append:

```csharp
    public delegate* unmanaged[Cdecl]<float*, float*, int> gameplay_input_get_move;
    public delegate* unmanaged[Cdecl]<int*, int> gameplay_input_was_jump_pressed;
```

`Native.cs` — extend `IsComplete`:

```csharp
        abi.gameplay_input_get_move != null &&
        abi.gameplay_input_was_jump_pressed != null;
```

Add wrappers:

```csharp
    public static int blunder_gameplay_input_get_move(float* outX, float* outY)
    {
        EnsureRegistered();
        return s_abi.gameplay_input_get_move(outX, outY);
    }

    public static int blunder_gameplay_input_was_jump_pressed(int* outPressed)
    {
        EnsureRegistered();
        return s_abi.gameplay_input_was_jump_pressed(outPressed);
    }
```

`Vec2.cs` (mirror `Vec3.cs` with X/Y only).

`Input.cs`:

```csharp
using System.Runtime.InteropServices;

namespace Blunder;

/// <summary>
/// Polls Gameplay Actions for the current simulation frame.
/// Authoritative only in Player Play Mode; otherwise idle.
/// </summary>
public static unsafe class Input
{
    public static Vec2 GetMove()
    {
        float x = 0f, y = 0f;
        if (Native.blunder_gameplay_input_get_move(&x, &y) != Native.Ok)
        {
            throw new InvalidOperationException("gameplay_input_get_move failed");
        }
        return new Vec2(x, y);
    }

    public static bool WasJumpPressed()
    {
        int pressed = 0;
        if (Native.blunder_gameplay_input_was_jump_pressed(&pressed) != Native.Ok)
        {
            throw new InvalidOperationException("gameplay_input_was_jump_pressed failed");
        }
        return pressed != 0;
    }
}
```

Ensure `Native` methods used by `Input` are `internal`/`public` as needed (`Input` is public; `Native` is `internal` — same assembly OK).

- [ ] **Step 4: Run managed tests**

```powershell
dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug
```

Expected: `Blunder.Api.NativeAbiTests: OK`.

Rebuild anything that embeds Api size assumptions (ScriptHost host start still works):

```powershell
cmake --build build/vs2026-debug --config Debug --target editor_dotnet_host_test dotnet_host_test
ctest --test-dir build/vs2026-debug -C Debug -R "native_abi|dotnet_host|editor_dotnet_host|gameplay_input" --output-on-failure
```

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add engine/managed/Blunder.Api/NativeAbi.cs engine/managed/Blunder.Api/Native.cs engine/managed/Blunder.Api/Vec2.cs engine/managed/Blunder.Api/Input.cs engine/managed/Blunder.Api.NativeAbiTests/Program.cs
git commit -m "$(cat <<'EOF'
feat: add Blunder.Api.Input façade over NativeAbi gameplay actions

EOF
)"
```

---

### Task 5: Docs cross-check + verification gate

**Files:**
- Modify only if names drifted: `CONTEXT.md`, `docs/adr/0015-gameplay-input-nativeabi-actions.md` (keep vocabulary aligned with `Input.GetMove` / `WasJumpPressed`)
- Optional: one-line pointer in OpenSpec tasks (already listed)

**Interfaces:** none new

- [ ] **Step 1: Skim CONTEXT + ADR against shipped API names**

Confirm: static `Input`, Move + Jump, Pause discard, unfocused idle, NativeAbi extension, WASD/Space, +X/+Y.

- [ ] **Step 2: Full verification commands**

```powershell
cmake --build build/vs2026-debug --config Debug --target gameplay_input_test native_abi_test engine_player
.\build\vs2026-debug\engine\src\tests\Debug\gameplay_input_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\native_abi_test.exe
dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug
ctest --test-dir build/vs2026-debug -C Debug -R "native_abi|dotnet_host|editor_dotnet_host|gameplay_input|play_pause" --output-on-failure
```

Expected: all green.

- [ ] **Step 3: Commit doc nits only if needed**

```bash
git add CONTEXT.md docs/adr/0015-gameplay-input-nativeabi-actions.md
git commit -m "$(cat <<'EOF'
docs: align Gameplay Input glossary with shipped Input API names

EOF
)"
```

If no doc changes, skip commit.

---

## Self-review

| Spec / grill requirement | Task |
|--------------------------|------|
| Player-only authoritative | 1 (non-player idle), 3 (hostMode sample) |
| Move + Jump Actions | 1, 4 |
| Jump press-edge shared | 1 (`current()` + held frame) |
| Pause discard / no buffer | 1, 3 (sample while paused) |
| Unfocused idle | 1, 3 |
| Static `Input` / NativeAbi extend | 2, 4 |
| WASD normalize / +X +Y | 1 |
| No GameCommand product path | 3 (explicit non-touch) |
| ABI v3 / completeness | 2, 4 |
| engine-c-abi / script-native-abi deltas | 2, 4 |

Placeholder scan: no TBD/TODO steps; code blocks included.

Type consistency: `blunder_gameplay_input_get_move` / `was_jump_pressed` / `Input.GetMove` / `WasJumpPressed` / `GameplayInputState::sample` used uniformly.
