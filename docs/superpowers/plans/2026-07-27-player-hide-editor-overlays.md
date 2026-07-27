# Player Hide Editor Overlays Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In Play Mode’s Player window, never draw or hit-test Editor Overlays (grid, Transform gizmo, Navigate gizmo, outline, axes, origins, wireframe); keep the editor viewport unchanged while a Play Session runs.

**Architecture:** Add a pure host-mode policy helper (same style as `play_tick_gate.h`). Gate `OverlaySystem` begin_sync/draw and `RenderSystem::onEvent` gizmo/nav paths on that policy when `EngineHostMode::Player`. Do not add an env override in this slice. Editor Camera orbit stays enabled (including Play Pause).

**Tech Stack:** C++20, `EngineHostMode`, OverlaySystem / RenderSystem, CTest unit tests (no Vulkan required for the policy test).

## Global Constraints

- Product rule lives in `CONTEXT.md` term **Editor Overlay** (Play section).
- Gate on **`EngineHostMode::Player` only** — never on “editor has an active Play Session”.
- Play Pause still hides Editor Overlays (Pause ≠ Edit Mode).
- Display **and** interaction off (no gizmo hover/click/drag in Player).
- No Player debug env to force overlays on in this slice.
- Editor Camera orbit of the Player view remains (already product language for Pause).
- TDD: failing test first for the policy helper; wire OverlaySystem/RenderSystem after.
- Scoped commits; do not commit unrelated WIP.
- Prefer OpenSpec change `player-hide-editor-overlays` before large apply (optional Task 0); plan is sufficient for a narrow fix if OpenSpec is deferred.

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/overlay/editor_overlay_policy.h` | Pure `editorOverlaysEnabled(EngineHostMode)` policy |
| `engine/src/tests/editor_overlay_policy_test.cpp` | Unit tests for the policy |
| `engine/src/tests/CMakeLists.txt` | Register the new test target |
| `engine/src/runtime/function/render/overlay/overlay_system.cpp` | Skip authorship overlay sync/draw in Player |
| `engine/src/runtime/function/render/overlay/overlay_system.h` | Optional `editorOverlaysActive()` helper on OverlaySystem |
| `engine/src/runtime/function/render/render_system.cpp` | Skip Transform/Navigate event handling in Player |
| `CONTEXT.md` | Already updated — do not regress the Editor Overlay glossary entry |

```
┌─────────────────────┐     ┌──────────────────────┐
│  engine_editor      │     │  engine_player       │
│  HostMode::Editor   │     │  HostMode::Player    │
│  overlays ON        │     │  overlays OFF        │
│  (even during Play  │     │  grid/gizmo/outline  │
│   Session)          │     │  draw+hit OFF        │
└─────────────────────┘     └──────────────────────┘
```

---

### Task 0 (optional): OpenSpec change scaffold

**Files:**
- Create: `openspec/changes/player-hide-editor-overlays/proposal.md`
- Create: `openspec/changes/player-hide-editor-overlays/design.md`
- Create: `openspec/changes/player-hide-editor-overlays/tasks.md`
- Create: `openspec/changes/player-hide-editor-overlays/specs/play-mode/spec.md` (delta)

**Interfaces:**
- Consumes: grilled decisions in this plan + `CONTEXT.md` **Editor Overlay**
- Produces: named change for `/opsx:apply` / archive later

- [ ] **Step 1: Create the change**

```bash
openspec new change "player-hide-editor-overlays"
```

- [ ] **Step 2: Write proposal (why / what / out of scope)**

Capture: Player-only hide of Editor Overlays; editor viewport unchanged; no env override; Pause still hidden; orbit kept.

- [ ] **Step 3: Write design + tasks mirroring Tasks 1–3 below**

- [ ] **Step 4: Commit OpenSpec-only (if using a dedicated change branch)**

```bash
git add openspec/changes/player-hide-editor-overlays
git commit -m "$(cat <<'EOF'
docs(openspec): propose player-hide-editor-overlays

EOF
)"
```

---

### Task 1: Policy helper + unit test

**Files:**
- Create: `engine/src/runtime/function/render/overlay/editor_overlay_policy.h`
- Create: `engine/src/tests/editor_overlay_policy_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt` (add target after `play_pause_tick_gate_test` block ~line 685)

**Interfaces:**
- Consumes: `EngineHostMode` from `runtime/function/global/engine_host_mode.h`
- Produces:
  ```cpp
  namespace Blunder {
  /// Editor Overlays (grid, gizmos, outline, …) are authorship chrome.
  /// Player never enables them — including while Play Pause is active.
  inline bool editorOverlaysEnabled(EngineHostMode host_mode) {
    return host_mode != EngineHostMode::Player;
  }
  }
  ```

- [ ] **Step 1: Write the failing test**

Create `engine/src/tests/editor_overlay_policy_test.cpp`:

```cpp
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/render/overlay/editor_overlay_policy.h"

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

int main() {
  using namespace Blunder;

  expect_true("editor enables overlays",
              editorOverlaysEnabled(EngineHostMode::Editor));
  expect_true("player disables overlays",
              !editorOverlaysEnabled(EngineHostMode::Player));

  // Pause is a separate flag; policy is host-mode only. Document that Pause
  // does not re-enable overlays by asserting Player stays false.
  expect_true("player still disabled (pause is orthogonal)",
              !editorOverlaysEnabled(EngineHostMode::Player));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("editor_overlay_policy_test: all passed\n");
  return 0;
}
```

- [ ] **Step 2: Register the test in CMake (still expect compile fail without header)**

In `engine/src/tests/CMakeLists.txt`, immediately after the `play_pause_tick_gate_test` `add_test` block, add:

```cmake
add_executable(editor_overlay_policy_test
    "editor_overlay_policy_test.cpp"
)

target_include_directories(editor_overlay_policy_test
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(MSVC)
    target_compile_options(editor_overlay_policy_test PRIVATE /Zc:preprocessor)
endif()

add_test(NAME editor_overlay_policy_test
         COMMAND editor_overlay_policy_test)
```

Note: this target does **not** need `engine_runtime` — header-only policy + `engine_host_mode.h`.

- [ ] **Step 3: Run test build to verify it fails (missing header)**

```powershell
cmake --build build/vs2026-debug --config Debug --target editor_overlay_policy_test
```

Expected: FAIL — cannot open `editor_overlay_policy.h`.

- [ ] **Step 4: Add minimal policy header**

Create `engine/src/runtime/function/render/overlay/editor_overlay_policy.h`:

```cpp
#pragma once

#include "runtime/function/global/engine_host_mode.h"

namespace Blunder {

/// Authorship viewport chrome (grid, Transform/Navigate gizmos, outline, …).
/// Disabled for the Player host — including while Play Pause is active.
inline bool editorOverlaysEnabled(EngineHostMode host_mode) {
  return host_mode != EngineHostMode::Player;
}

}  // namespace Blunder
```

- [ ] **Step 5: Build and run the test**

```powershell
cmake --build build/vs2026-debug --config Debug --target editor_overlay_policy_test
ctest --test-dir build/vs2026-debug -C Debug -R editor_overlay_policy_test --output-on-failure
```

Expected: PASS — `editor_overlay_policy_test: all passed`

- [ ] **Step 6: Commit**

```bash
git add engine/src/runtime/function/render/overlay/editor_overlay_policy.h \
        engine/src/tests/editor_overlay_policy_test.cpp \
        engine/src/tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
test(overlay): add editorOverlaysEnabled host-mode policy

EOF
)"
```

---

### Task 2: Gate OverlaySystem sync and draw

**Files:**
- Modify: `engine/src/runtime/function/render/overlay/overlay_system.h`
- Modify: `engine/src/runtime/function/render/overlay/overlay_system.cpp`

**Interfaces:**
- Consumes: `editorOverlaysEnabled(g_runtime_global_context.hostMode())`
- Produces: When false, all authorship overlays stay `enabled_ == false` and draw paths no-op; screen pass not begun

- [ ] **Step 1: Add a private helper declaration on OverlaySystem**

In `overlay_system.h`, in the `private:` section:

```cpp
  bool authorshipOverlaysActive() const;
  void disableAuthorshipOverlays();
```

- [ ] **Step 2: Implement disable + active check**

In `overlay_system.cpp`, add include:

```cpp
#include "runtime/function/render/overlay/editor_overlay_policy.h"
```

Implement:

```cpp
bool OverlaySystem::authorshipOverlaysActive() const {
  return editorOverlaysEnabled(g_runtime_global_context.hostMode());
}

void OverlaySystem::disableAuthorshipOverlays() {
  m_grid.enabled_ = false;
  m_axes.enabled_ = false;
  m_wireframe.enabled_ = false;
  m_origins.enabled_ = false;
  m_outline.enabled_ = false;
  m_navigate_gizmo.enabled_ = false;
  m_transform_gizmo.enabled_ = false;
  m_anti_aliasing.enabled_ = false;
}
```

Note: `enabled_` is a public data member on `Overlay` (`overlay_base.h`). Prefer setting it directly here rather than adding setters — matches existing overlay style.

- [ ] **Step 3: Gate `begin_sync`**

At the top of `OverlaySystem::begin_sync`, after building `m_state` is optional; clearest pattern:

```cpp
void OverlaySystem::begin_sync(const ForwardFrameState& frame_state,
                               uint32_t current_frame) {
  m_state = OverlayState::fromFrameState(frame_state, current_frame);
  m_state.gizmo_mode = m_transform_gizmo.controller().getMode();
  m_state.gizmo_space = m_transform_gizmo.controller().getSpace();

  if (!authorshipOverlaysActive()) {
    m_state.has_selection = false;
    disableAuthorshipOverlays();
    return;
  }

  // ... existing selection + begin_sync calls unchanged ...
}
```

This prevents `GridOverlay` / `NavigateGizmoOverlay` from forcing `enabled_ = true` in Player.

- [ ] **Step 4: Gate draw entry points**

At the start of each of:

- `draw_scene_overlays`
- `draw_outline`
- `draw_overlay_lines`
- `draw_overlay_aa`
- `draw_screen_overlays`

add:

```cpp
  if (!authorshipOverlaysActive()) {
    return;
  }
```

For `draw_screen_overlays`, return **before** `m_screen_pass.begin(cmd)` so Player does not open an empty screen overlay pass.

- [ ] **Step 5: Build runtime + player**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_player
```

Expected: SUCCESS.

- [ ] **Step 6: Manual smoke (Player only)**

```powershell
$env:BLUNDER_PLAYER_MAX_FRAMES='90'
& .\build\vs2026-debug\bin\Debug\engine_player.exe `
  --project-root "E:\Blunder Projects\Test" `
  --scene "assets/Scenes/pick_test.scene.asset"
```

Expected: window shows scene meshes; **no** ground grid, Transform gizmo, or Navigate gizmo. Log may still show renderer init.

- [ ] **Step 7: Commit**

```bash
git add engine/src/runtime/function/render/overlay/overlay_system.h \
        engine/src/runtime/function/render/overlay/overlay_system.cpp
git commit -m "$(cat <<'EOF'
fix(player): disable Editor Overlay draw in Player host

EOF
)"
```

---

### Task 3: Gate Transform / Navigate interaction in RenderSystem

**Files:**
- Modify: `engine/src/runtime/function/render/render_system.cpp` (`RenderSystem::onEvent`, ~1254+)

**Interfaces:**
- Consumes: `editorOverlaysEnabled(g_runtime_global_context.hostMode())`
- Produces: Player never marks events handled via Transform/Navigate gizmo paths

- [ ] **Step 1: Include policy**

Near other includes in `render_system.cpp`:

```cpp
#include "runtime/function/render/overlay/editor_overlay_policy.h"
```

(Ensure `global_context.h` is already included — it is.)

- [ ] **Step 2: Wrap gizmo/nav event blocks**

In `RenderSystem::onEvent`, wrap the three authorship blocks (transform `controller().onEvent`, mouse-move hover for transform+nav, left-click `navigate_gizmo().tryHandleMouseClick`) so they only run when overlays are enabled:

```cpp
  const bool overlays =
      editorOverlaysEnabled(g_runtime_global_context.hostMode());

  if (overlays && m_overlay_system && m_editor_camera) {
    m_overlay_system->transform_gizmo().controller().onEvent(event,
                                                             *m_editor_camera);
    if (event.handled) {
      return;
    }
  }

  if (overlays && m_overlay_system && m_editor_camera &&
      event.getEventType() == EventType::MouseMoved && !event.handled) {
    // ... existing hover update bodies unchanged ...
  }

  if (overlays && m_overlay_system && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    // ... existing navigate click + any transform press handling unchanged ...
  }
```

Do **not** wrap RenderDoc F11 or Editor Camera orbit / other non-overlay input.

- [ ] **Step 3: Build editor + player**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_player engine_editor
```

Expected: SUCCESS.

- [ ] **Step 4: Manual dual-window check**

1. Launch `engine_editor` with the Test project.
2. Confirm editor viewport still shows grid + Navigate gizmo.
3. Press Play; focus Player window — no grid / Transform / Navigate.
4. Pause from editor — Player still no Editor Overlays; orbit still works if product orbit bindings apply.
5. Stop Play — editor overlays unchanged.

- [ ] **Step 5: Re-run policy unit test**

```powershell
ctest --test-dir build/vs2026-debug -C Debug -R editor_overlay_policy_test --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add engine/src/runtime/function/render/render_system.cpp
git commit -m "$(cat <<'EOF'
fix(player): ignore Editor Overlay gizmo input in Player host

EOF
)"
```

---

### Task 4: Docs / glossary sanity (no product copy churn)

**Files:**
- Verify only: `CONTEXT.md` (**Editor Overlay** entry under Play)

- [ ] **Step 1: Read the glossary entry**

Confirm it still states: Player never shows/interacts; editor viewport keeps overlays during Play Session; Pause still hidden; no force-on env; orbit is not an Editor Overlay.

- [ ] **Step 2: Commit only if CONTEXT.md drifted during implementation**

```bash
git add CONTEXT.md
git commit -m "$(cat <<'EOF'
docs: keep Editor Overlay glossary aligned with Player gate

EOF
)"
```

Skip this commit if `CONTEXT.md` is already correct (already written during grilling).

---

## Self-review

**Spec coverage**

| Requirement | Task |
|-------------|------|
| Player-only hide | Task 2–3 via `EngineHostMode::Player` |
| Editor viewport unchanged during Play Session | Implicit (editor host still enables) — Task 3 manual check |
| Full Editor Overlay set (grid, transform, nav, outline, axes, origins, wireframe) | Task 2 `disableAuthorshipOverlays` + gated draws |
| Pause still hidden | Policy is host-mode only (Task 1 asserts) |
| Display + interaction off | Task 2 draw + Task 3 events |
| No env override | No env reads in any task |
| Orbit kept | Task 3 does not gate camera orbit |

**Placeholder scan:** None intentional.

**Type consistency:** Single symbol `editorOverlaysEnabled(EngineHostMode)` used by OverlaySystem and RenderSystem.

**Out of scope (do not implement):** Skipping OverlaySystem GPU init in Player; zero-copy present; changing editor overlays when Play Session is active; Player HUD; debug env.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-27-player-hide-editor-overlays.md`.

**Two execution options:**

1. **Subagent-Driven (recommended)** — fresh subagent per task, review between tasks  
2. **Inline Execution** — execute in this session with executing-plans checkpoints  

**Which approach?**

Optional first: create OpenSpec change `player-hide-editor-overlays` (Task 0) before coding — say if you want that scaffolded now.
