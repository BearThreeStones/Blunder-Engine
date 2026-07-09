# Fix Transform Live Viewport Redraw Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When the user edits a selected entity's transform via gizmo drag or Inspector fields, the viewport mesh and gizmo handles update immediately without requiring a camera move.

**Architecture:** Entity TRS and `syncSceneToRender` already update every frame; `tickVulkan` skips the offscreen pass when camera matrices are unchanged and `m_viewport_render_generation` / `m_force_viewport_render` are idle. Wire transform-edit paths to call `RenderSystem::requestViewportRedraw()` (same contract as selection and gizmo mode/space). Centralize the gizmo side inside the existing anonymous `markSceneDirty()` helper so drag, release, and Escape cancel cannot drift apart. Mirror the selection present path in `SlintSystem::applyInspectorTransform` (`markViewportDirtyRegion` + `requestViewportRedraw`).

**Tech Stack:** C++20, `RenderSystem::requestViewportRedraw`, `SlintSystem::markViewportDirtyRegion`, existing editor frame loop (`syncSceneToRender` → `tickVulkan` skip gate).

**OpenSpec (recommended before code):** `/opsx:propose fix-transform-live-viewport-redraw` — small delta on viewport/transform-gizmo behavior (transform edits force redraw under static camera).

**Confirmed symptoms (explore):**
1. Inspector numbers update live during gizmo drag (`syncInspectorLive` works).
2. Mesh and gizmo handles stay frozen until the camera moves (`camera_changed` opens the skip gate).

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/transform_edit_viewport_notify.h` | Free helpers: gizmo/inspector transform-edit → viewport redraw (unit-testable) |
| `engine/src/runtime/function/render/transform_edit_viewport_notify.cpp` | Call `requestViewportRedraw` / optional `markViewportDirtyRegion` |
| `engine/src/tests/transform_edit_viewport_notify_test.cpp` | TDD: generation bumps after notify helpers |
| `engine/src/tests/CMakeLists.txt` | Register new test |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp` | `markSceneDirty()` → `notifyViewportAfterGizmoTransformEdit` |
| `engine/src/runtime/function/slint/slint_system.cpp` | `applyInspectorTransform` → `notifyViewportAfterInspectorTransformEdit` |
| `docs/agents/render-pipeline.md` | Document transform-edit → redraw contract next to selection present row |
| `openspec/changes/fix-transform-live-viewport-redraw/` | Proposal / design / tasks / spec delta (via `/opsx:propose`) |

**Out of scope:** Changing `tickVulkan` skip heuristics; interactive pacing tier tuning; draw-list caching; undo/redo; multi-select transform.

**TDD note:** Production wiring goes through the notify helpers. Watch the unit test fail before implementing the helpers; then wire call sites.

---

### Task 1: OpenSpec change skeleton

**Files:**
- Create via CLI: `openspec/changes/fix-transform-live-viewport-redraw/`

- [ ] **Step 1: Propose the change**

```powershell
openspec new change "fix-transform-live-viewport-redraw"
```

Then fill artifacts (or run `/opsx:propose fix-transform-live-viewport-redraw`) with this intent:

- **Problem:** Transform edits update entity + inspector but not the viewport until camera moves, because `requestViewportRedraw` is missing on gizmo drag and Inspector apply.
- **Fix:** Call `requestViewportRedraw` from gizmo `markSceneDirty`; call `markViewportDirtyRegion` + `requestViewportRedraw` from `applyInspectorTransform`.
- **Spec delta** (capability `transform-gizmo` or `editor-viewport-pacing` — pick the one that already owns redraw-on-edit language):

```markdown
### Requirement: Transform edits force viewport redraw

When the user changes a selected entity's transform via gizmo drag or Inspector TRS fields, the editor SHALL request a viewport redraw so mesh and gizmo overlays update without camera motion.

#### Scenario: Gizmo translate drag under static camera

- **WHEN** a selected entity is dragged with the translate gizmo
- **AND** the editor camera matrices do not change
- **THEN** the viewport mesh and gizmo handles SHALL move with the drag on subsequent frames

#### Scenario: Inspector position edit under static camera

- **WHEN** the user edits Inspector position/rotation/scale for the selection
- **AND** the editor camera matrices do not change
- **THEN** the viewport mesh and gizmo handles SHALL update to the new transform without requiring a camera move
```

- [ ] **Step 2: Commit OpenSpec artifacts**

```powershell
git add openspec/changes/fix-transform-live-viewport-redraw
git commit -m "$(cat <<'EOF'
docs: propose fix-transform-live-viewport-redraw

EOF
)"
```

---

### Task 2: TDD notify helpers + gizmo `markSceneDirty` wiring

**Files:**
- Create: `engine/src/runtime/function/render/transform_edit_viewport_notify.h`
- Create: `engine/src/runtime/function/render/transform_edit_viewport_notify.cpp`
- Create: `engine/src/tests/transform_edit_viewport_notify_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: engine runtime CMake so the new `.cpp` is compiled (find where `transform_gizmo_controller.cpp` is listed and add the sibling file the same way)
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp` (anonymous `markSceneDirty`)

- [ ] **Step 1: Write the failing test (RED)**

Create `engine/src/tests/transform_edit_viewport_notify_test.cpp`:

```cpp
#include <cstdio>

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/transform_edit_viewport_notify.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool value) {
  if (!value) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    RenderSystem render;
    const uint64_t before = render.getViewportRenderGeneration();
    notifyViewportAfterGizmoTransformEdit(&render);
    expect_true("gizmo notify bumps viewport generation",
                render.getViewportRenderGeneration() == before + 1);
  }

  {
    RenderSystem render;
    const uint64_t before = render.getViewportRenderGeneration();
    notifyViewportAfterGizmoTransformEdit(nullptr);
    expect_true("gizmo notify null render is no-op",
                render.getViewportRenderGeneration() == before);
  }

  {
    RenderSystem render;
    const uint64_t before = render.getViewportRenderGeneration();
    // SlintSystem is optional for the unit test: null skips markViewportDirtyRegion
    // but must still force the Vulkan redraw path.
    notifyViewportAfterInspectorTransformEdit(&render, nullptr);
    expect_true("inspector notify bumps viewport generation with null slint",
                render.getViewportRenderGeneration() == before + 1);
  }

  if (g_failures == 0) {
    std::printf("transform_edit_viewport_notify_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "transform_edit_viewport_notify_test: %d failure(s)\n",
               g_failures);
  return 1;
}
```

Register in `engine/src/tests/CMakeLists.txt` (same pattern as `transform_gizmo_hover_test`):

```cmake
add_executable(transform_edit_viewport_notify_test transform_edit_viewport_notify_test.cpp)

target_link_libraries(transform_edit_viewport_notify_test PRIVATE engine_runtime cgltf)

if(MSVC)
  target_compile_options(transform_edit_viewport_notify_test PRIVATE /Zc:preprocessor)
endif()

add_test(NAME transform_edit_viewport_notify_test COMMAND transform_edit_viewport_notify_test)
set_tests_properties(transform_edit_viewport_notify_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/engine/src/editor/Debug"
)
```

Declare the API in the header (no bodies yet — or declare only so the test fails to link / fails assertions after stub):

```cpp
#pragma once

namespace Blunder {

class RenderSystem;
class SlintSystem;

void notifyViewportAfterGizmoTransformEdit(RenderSystem* render_system);
void notifyViewportAfterInspectorTransformEdit(RenderSystem* render_system,
                                               SlintSystem* slint_system);

}  // namespace Blunder
```

- [ ] **Step 2: Run test to verify RED**

Reconfigure if needed, then:

```powershell
cmake --build build/vs2026-debug --config Debug --target transform_edit_viewport_notify_test
ctest --test-dir build/vs2026-debug -C Debug -R transform_edit_viewport_notify_test --output-on-failure
```

Expected: link failure (missing symbols) **or** test FAIL because stubs do not bump generation. Do **not** implement real bodies until you see the failure.

- [ ] **Step 3: Minimal GREEN implementation**

`transform_edit_viewport_notify.cpp`:

```cpp
#include "runtime/function/render/transform_edit_viewport_notify.h"

#include "runtime/function/render/render_system.h"
#include "runtime/function/slint/slint_system.h"

namespace Blunder {

void notifyViewportAfterGizmoTransformEdit(RenderSystem* render_system) {
  if (render_system != nullptr) {
    render_system->requestViewportRedraw();
  }
}

void notifyViewportAfterInspectorTransformEdit(RenderSystem* render_system,
                                               SlintSystem* slint_system) {
  if (slint_system != nullptr) {
    slint_system->markViewportDirtyRegion();
  }
  if (render_system != nullptr) {
    render_system->requestViewportRedraw();
  }
}

}  // namespace Blunder
```

Add the `.cpp` to the same CMake target that compiles other `engine/src/runtime/function/render/*.cpp` sources (mirror how `scene_render_bridge.cpp` / gizmo sources are listed).

- [ ] **Step 4: Run test to verify GREEN**

```powershell
cmake --build build/vs2026-debug --config Debug --target transform_edit_viewport_notify_test
ctest --test-dir build/vs2026-debug -C Debug -R transform_edit_viewport_notify_test --output-on-failure
```

Expected: PASS (`transform_edit_viewport_notify_test: all passed`).

- [ ] **Step 5: Wire gizmo `markSceneDirty`**

Confirm call sites still go through `markSceneDirty()`:

1. `onMouseMoved` after successful TRS apply
2. `onMouseReleased` after ending drag
3. Escape cancel after restoring TRS

Replace the helper body with:

```cpp
void markSceneDirty() {
  if (SceneInstance* scene = activeScene()) {
    scene->markTransformsDirty();
  }
  if (g_runtime_global_context.m_editor_scene_edit) {
    g_runtime_global_context.m_editor_scene_edit->markDirty();
  }
  notifyViewportAfterGizmoTransformEdit(
      g_runtime_global_context.m_render_system.get());
}
```

Add `#include "runtime/function/render/transform_edit_viewport_notify.h"`.

- [ ] **Step 6: Build runtime + re-run test**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_runtime
ctest --test-dir build/vs2026-debug -C Debug -R transform_edit_viewport_notify_test --output-on-failure
```

- [ ] **Step 7: Commit**

```powershell
git add engine/src/runtime/function/render/transform_edit_viewport_notify.h engine/src/runtime/function/render/transform_edit_viewport_notify.cpp engine/src/tests/transform_edit_viewport_notify_test.cpp engine/src/tests/CMakeLists.txt engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp
# also add whatever CMakeLists listed the new .cpp
git commit -m "$(cat <<'EOF'
fix: request viewport redraw on gizmo transform dirty

EOF
)"
```

---

### Task 3: Inspector transform apply uses notify helper

**Files:**
- Modify: `engine/src/runtime/function/slint/slint_system.cpp` (`SlintSystem::applyInspectorTransform`)

- [ ] **Step 1: Confirm Task 2 inspector unit case already covers the helper (GREEN)**

```powershell
ctest --test-dir build/vs2026-debug -C Debug -R transform_edit_viewport_notify_test --output-on-failure
```

Expected: still PASS. Do not change production inspector code until this is green.

- [ ] **Step 2: Wire `applyInspectorTransform`**

Inside the `try` block, after `markTransformsDirty` / `editor_scene_edit->markDirty()`:

```cpp
    scene->markTransformsDirty();
    if (services->editor_scene_edit) {
      services->editor_scene_edit->markDirty();
    }
    notifyViewportAfterInspectorTransformEdit(services->render_system.get(), this);
```

Add `#include "runtime/function/render/transform_edit_viewport_notify.h"` if not already visible.

Prefer `services->render_system.get()` over a global fallback.

- [ ] **Step 3: Build editor + re-run unit test**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
ctest --test-dir build/vs2026-debug -C Debug -R transform_edit_viewport_notify_test --output-on-failure
```

Expected: build succeeds; test still PASS.

- [ ] **Step 4: Commit**

```powershell
git add engine/src/runtime/function/slint/slint_system.cpp
git commit -m "$(cat <<'EOF'
fix: redraw viewport when inspector transform is applied

EOF
)"
```

---

### Task 4: Docs + manual QA

**Files:**
- Modify: `docs/agents/render-pipeline.md` (GPU pick / selection present table ~line 110)

- [ ] **Step 1: Document the contract**

In the table that already has:

```markdown
| Selection present | `onSelectionChanged` → `markViewportDirtyRegion()` + `requestViewportRedraw` |
```

Add a sibling row:

```markdown
| Transform edit (gizmo / Inspector) | Gizmo `markSceneDirty()` → `requestViewportRedraw`; Inspector `applyInspectorTransform` → `markViewportDirtyRegion()` + `requestViewportRedraw` (static camera must not skip the offscreen pass) |
```

- [ ] **Step 2: Manual QA (required — no automated viewport harness)**

Run:

```powershell
./build/vs2026-debug/engine/src/editor/Debug/engine_editor.exe
```

Default scene `assets/Scenes/pick_test.scene.asset` is fine. Checklist:

1. Select a mesh; enable translate (G or toolbar). **Do not** orbit/pan the camera after selection settles.
2. Drag a translate axis handle — mesh **and** gizmo handles move continuously; Inspector numbers keep updating.
3. Release mouse — final pose stays; still no camera move needed.
4. Edit Inspector position X by typing / spinner — mesh and gizmo jump to the new pose without camera move.
5. Repeat for rotate (R) and scale (S) drag under a static camera.
6. During a translate drag, press Escape — mesh/gizmo snap back to drag-start pose without camera move.
7. Sanity: orbit camera still works; selection outline still appears on pick without camera move (existing pick fix).

- [ ] **Step 3: Mark OpenSpec tasks complete / validate**

```powershell
openspec validate fix-transform-live-viewport-redraw
```

Update `openspec/changes/fix-transform-live-viewport-redraw/tasks.md` checkboxes to match completed work.

- [ ] **Step 4: Commit docs**

```powershell
git add docs/agents/render-pipeline.md openspec/changes/fix-transform-live-viewport-redraw
git commit -m "$(cat <<'EOF'
docs: transform-edit viewport redraw contract

EOF
)"
```

---

## Self-review

| Spec / symptom | Task |
|----------------|------|
| Gizmo drag updates mesh + handles under static camera | Task 2 (`markSceneDirty` → `requestViewportRedraw`) |
| Inspector TRS edit updates mesh + handles under static camera | Task 3 |
| Escape cancel restores pose visually | Task 2 (Escape already calls `markSceneDirty`) |
| Mouse release after drag does not leave a stale frame | Task 2 (`onMouseReleased` calls `markSceneDirty`) |
| Inspector numbers already live (no regression) | Unchanged `syncInspectorLive`; Task 2 only adds redraw |
| Docs / OpenSpec | Tasks 1 + 4 |

**Placeholder scan:** No TBD/TODO steps; exact snippets and commands included.

**Type consistency:** Uses existing `RenderSystem::requestViewportRedraw()`, `SlintSystem::markViewportDirtyRegion()`, `SceneInstance::markTransformsDirty()`, `UiContext::LockedServices::render_system`.

**Why not change the skip gate?** `syncSceneToRender` already rebuilds draw lists every frame; forcing a full re-render whenever draw lists exist would defeat idle pacing. The bug is missing dirty signaling on transform edits, not the skip heuristic itself.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-09-fix-transform-live-viewport-redraw.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
