# Gizmo Handle Hover Highlight Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When the mouse hovers over an interactive transform gizmo handle, highlight that handle using the existing `color_hi` path (Blender `WM_GIZMO_DRAW_HOVER` parity).

**Architecture:** Reuse the existing CPU analytic pick functions (`pickTranslationGizmoHandle`, etc.) on `MouseMoved` when not dragging. Store `m_hover_axis` on `TransformGizmoController`. Overlay already supports per-handle `highlight` → `gizmoAxisColor().color_hi`; extend that flag to cover hover as well as active drag. Request viewport redraw only when the hovered axis changes.

**Tech Stack:** C++20, existing gizmo pick/overlay (`transform_gizmo_pick.*`, `transform_gizmo_overlay.cpp`), `RenderSystem::onEvent`, `GizmoAxisColor` / `TransformGizmoMetrics`.

**OpenSpec (recommended before code):** `/opsx:propose gizmo-handle-hover` → delta on `openspec/specs/transform-gizmo/spec.md`.

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.h` | `pickTransformGizmoHandle(mode, ctx)` + `gizmoHandleHighlighted(active, hover, axis)` |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.cpp` | Mode-dispatching pick wrapper |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.h` | `m_hover_axis`, hover API, `buildPickContext` |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp` | Hover update/clear; refactor press pick to shared context builder |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` | Replace inline drag-only highlight checks with `isHandleHighlighted(axis)` |
| `engine/src/runtime/function/render/render_system.cpp` | Wire `MouseMoved` → hover update + redraw on change |
| `engine/src/tests/transform_gizmo_hover_test.cpp` | Unit tests for highlight helper + pick wrapper |
| `engine/src/tests/CMakeLists.txt` | Register new test executable |
| `docs/agents/render-pipeline.md` | Document hover input path (optional one paragraph) |

**Out of scope:** Cursor shape changes, hover on non-interactive decor (`rot_t` trackball, `rot_c` outer ring, `scale_c_outer` annulus — not in pick paths today), navigate gizmo hover.

---

## Task 1: Highlight helper + pick wrapper

**Files:**
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.h`
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_pick.cpp`
- Create: `engine/src/tests/transform_gizmo_hover_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `engine/src/tests/transform_gizmo_hover_test.cpp`:

```cpp
#include <cstdio>
#include <optional>

#include <glm/vec3.hpp>

#include "runtime/core/math/geometry.h"
#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/gizmo/transform_gizmo_pick.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool value) {
  if (!value) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_axis(const char* label,
                 const std::optional<Blunder::ManipulatorAxis>& actual,
                 Blunder::ManipulatorAxis expected) {
  if (!actual || *actual != expected) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

Blunder::TransformGizmoPickContext makeTranslatePickCtx() {
  Blunder::TransformGizmoPickContext ctx{};
  ctx.ray = Blunder::Ray{glm::vec3(-2.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
  ctx.basis.origin = glm::vec3(0.0f);
  ctx.basis.axis_x = glm::vec3(1.0f, 0.0f, 0.0f);
  ctx.basis.axis_y = glm::vec3(0.0f, 1.0f, 0.0f);
  ctx.basis.axis_z = glm::vec3(0.0f, 0.0f, 1.0f);
  ctx.group_scale = 1.0f;
  ctx.gizmo_pixel_size = 0.01f;
  ctx.camera_forward = glm::vec3(0.0f, 0.0f, -1.0f);
  ctx.camera_position = glm::vec3(0.0f, 0.0f, 5.0f);
  return ctx;
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_true("drag highlights active axis",
              gizmoHandleHighlighted(ManipulatorAxis::trans_y, ManipulatorAxis::trans_x,
                                   ManipulatorAxis::trans_y));
  expect_true("hover highlights when not dragging",
              gizmoHandleHighlighted(std::nullopt, ManipulatorAxis::trans_z,
                                     ManipulatorAxis::trans_z));
  expect_true("no highlight when idle",
              !gizmoHandleHighlighted(std::nullopt, std::nullopt, ManipulatorAxis::trans_x));

  const auto ctx = makeTranslatePickCtx();
  expect_axis("pick wrapper hits X arrow",
              pickTransformGizmoHandle(TransformGizmoMode::translate, ctx),
              ManipulatorAxis::trans_x);
  expect_axis("pick wrapper misses in none mode",
              pickTransformGizmoHandle(TransformGizmoMode::none, ctx),
              ManipulatorAxis::trans_x);  // expect miss — test adjusted below

  if (g_failures == 0) {
    std::printf("transform_gizmo_hover_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "transform_gizmo_hover_test: %d failure(s)\n", g_failures);
  return 1;
}
```

Fix the last case to expect miss:

```cpp
  {
    const auto hit = pickTransformGizmoHandle(TransformGizmoMode::none, ctx);
    expect_true("pick wrapper misses in none mode", !hit.has_value());
  }
```

- [ ] **Step 2: Register test in CMake**

Add to `engine/src/tests/CMakeLists.txt` (mirror `pick_entity_promotion_test`):

```cmake
add_executable(transform_gizmo_hover_test transform_gizmo_hover_test.cpp)

target_link_libraries(transform_gizmo_hover_test PRIVATE engine_runtime cgltf)

if(MSVC)
  target_compile_options(transform_gizmo_hover_test PRIVATE /Zc:preprocessor)
endif()

add_test(NAME transform_gizmo_hover_test COMMAND transform_gizmo_hover_test)
set_tests_properties(transform_gizmo_hover_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/engine/src/editor/Debug"
)
```

- [ ] **Step 3: Run test to verify it fails**

Run:

```powershell
cmake --build build/vs2026-debug --target transform_gizmo_hover_test --config Debug
build\vs2026-debug\engine\src\tests\Debug\transform_gizmo_hover_test.exe
```

Expected: link/compile error — `gizmoHandleHighlighted` / `pickTransformGizmoHandle` not defined.

- [ ] **Step 4: Implement helpers**

In `transform_gizmo_pick.h` add:

```cpp
bool gizmoHandleHighlighted(std::optional<ManipulatorAxis> active_axis,
                            std::optional<ManipulatorAxis> hover_axis,
                            ManipulatorAxis axis);

std::optional<ManipulatorAxis> pickTransformGizmoHandle(
    TransformGizmoMode mode, const TransformGizmoPickContext& ctx);
```

In `transform_gizmo_pick.cpp` add:

```cpp
bool gizmoHandleHighlighted(const std::optional<ManipulatorAxis> active_axis,
                            const std::optional<ManipulatorAxis> hover_axis,
                            const ManipulatorAxis axis) {
  if (active_axis) {
    return *active_axis == axis;
  }
  return hover_axis && *hover_axis == axis;
}

std::optional<ManipulatorAxis> pickTransformGizmoHandle(
    const TransformGizmoMode mode, const TransformGizmoPickContext& ctx) {
  switch (mode) {
    case TransformGizmoMode::translate:
      return pickTranslationGizmoHandle(ctx);
    case TransformGizmoMode::rotate:
      return pickRotationGizmoHandle(ctx);
    case TransformGizmoMode::scale:
      return pickScaleGizmoHandle(ctx);
    case TransformGizmoMode::none:
    default:
      return std::nullopt;
  }
}
```

- [ ] **Step 5: Run test to verify it passes**

Run the same build + exe commands. Expected: `transform_gizmo_hover_test: all passed`.

- [ ] **Step 6: Commit**

```bash
git add engine/src/runtime/function/render/gizmo/transform_gizmo_pick.h \
        engine/src/runtime/function/render/gizmo/transform_gizmo_pick.cpp \
        engine/src/tests/transform_gizmo_hover_test.cpp \
        engine/src/tests/CMakeLists.txt
git commit -m "test: add gizmo hover highlight helpers and pick wrapper"
```

---

## Task 2: Controller hover state

**Files:**
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.h`
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp`

- [ ] **Step 1: Extend controller API**

In `transform_gizmo_controller.h` add public methods:

```cpp
bool hasHover() const { return m_hover_axis.has_value(); }
std::optional<ManipulatorAxis> getHoverAxis() const { return m_hover_axis; }
bool isHandleHighlighted(ManipulatorAxis axis) const;

/// Returns true if hover axis changed (caller should redraw).
bool updateHoverFromPointer(const Vec2& window_pos, EditorCamera& camera);
void clearHover();
```

Add private:

```cpp
std::optional<ManipulatorAxis> m_hover_axis;
bool buildPickContext(const Vec2& window_pos, EditorCamera& camera,
                      TransformGizmoPickContext& out_ctx,
                      float& out_group_scale) const;
```

- [ ] **Step 2: Implement hover logic**

In `transform_gizmo_controller.cpp`:

```cpp
bool TransformGizmoController::isHandleHighlighted(const ManipulatorAxis axis) const {
  return gizmoHandleHighlighted(m_active_axis, m_hover_axis, axis);
}

void TransformGizmoController::clearHover() {
  m_hover_axis.reset();
}

bool TransformGizmoController::updateHoverFromPointer(const Vec2& window_pos,
                                                    EditorCamera& camera) {
  if (isDragging() || m_mode == TransformGizmoMode::none) {
    const bool had_hover = m_hover_axis.has_value();
    clearHover();
    return had_hover;
  }
  if (!camera.isWindowPositionInViewport(window_pos)) {
    const bool had_hover = m_hover_axis.has_value();
    clearHover();
    return had_hover;
  }

  TransformGizmoPickContext pick_ctx{};
  float group_scale = 1.0f;
  if (!buildPickContext(window_pos, camera, pick_ctx, group_scale)) {
    const bool had_hover = m_hover_axis.has_value();
    clearHover();
    return had_hover;
  }

  const std::optional<ManipulatorAxis> hit =
      pickTransformGizmoHandle(m_mode, pick_ctx);
  if (hit == m_hover_axis) {
    return false;
  }
  m_hover_axis = hit;
  return true;
}
```

- [ ] **Step 3: Extract `buildPickContext` from `onMousePressed`**

Refactor existing press path to call `buildPickContext` instead of duplicating scale/pick setup (lines ~224–245 in `transform_gizmo_controller.cpp`). `buildPickContext` returns `false` when mode/selection/basis invalid; otherwise fills `TransformGizmoPickContext` and `group_scale` (store into `m_gizmo_group_scale` on press).

- [ ] **Step 4: Clear hover on mode/cancel/drag start**

In `setMode`, `cancelInteraction`, and successful `onMousePressed` (when drag starts):

```cpp
clearHover();
```

- [ ] **Step 5: Build**

```powershell
cmake --build build/vs2026-debug --target engine_editor --config Debug
```

Expected: compiles without errors.

- [ ] **Step 6: Commit**

```bash
git add engine/src/runtime/function/render/gizmo/transform_gizmo_controller.h \
        engine/src/runtime/function/render/gizmo/transform_gizmo_controller.cpp
git commit -m "feat: track transform gizmo hover axis on controller"
```

---

## Task 3: Overlay highlight wiring

**Files:**
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp`

- [ ] **Step 1: Replace drag-only highlight checks**

In `recordGizmoDraw`, replace every pattern:

```cpp
const bool highlight =
    m_controller.isDragging() && m_controller.getActiveAxis() == axis;
```

with:

```cpp
const bool highlight = m_controller.isHandleHighlighted(axis);
```

Apply to translate, scale (planes, axes, center), and rotate axis dials. Keep the separate ghost dial branch (`isDragging() && isRotationManipulator(...)`) unchanged.

- [ ] **Step 2: Build and smoke test**

```powershell
cmake --build build/vs2026-debug --target engine_editor --config Debug
```

Launch editor, select entity, press `G`, move mouse over X arrow — arrow should brighten (`color_hi`, alpha 1.0).

- [ ] **Step 3: Commit**

```bash
git add engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp
git commit -m "feat: apply gizmo hover highlight in overlay draw path"
```

---

## Task 4: Input wiring in RenderSystem

**Files:**
- Modify: `engine/src/runtime/function/render/render_system.cpp`

- [ ] **Step 1: Handle MouseMoved for hover**

In `RenderSystem::onEvent`, after transform gizmo `onEvent` block and **before** viewport pick pointer move, add:

```cpp
  if (m_overlay_system && m_editor_camera &&
      event.getEventType() == EventType::MouseMoved) {
    auto& mouse_event = static_cast<MouseMovedEvent&>(event);
    if (mouse_event.hasMousePosition()) {
      auto& gizmo_ctrl = m_overlay_system->transform_gizmo().controller();
      const Vec2 pos(mouse_event.getX(), mouse_event.getY());
      const bool in_viewport =
          m_editor_camera->isWindowPositionInViewport(pos);
      bool hover_changed = false;
      if (gizmo_ctrl.getMode() != TransformGizmoMode::none && !gizmo_ctrl.isDragging() &&
          in_viewport) {
        hover_changed = gizmo_ctrl.updateHoverFromPointer(pos, *m_editor_camera);
      } else if (gizmo_ctrl.hasHover()) {
        gizmo_ctrl.clearHover();
        hover_changed = true;
      }
      if (hover_changed) {
        requestViewportRedraw();
      }
    }
  }
```

**Note:** Do **not** set `event.handled` — hover must not block `ViewportPickSystem::onViewportPointerMoved` or camera input.

- [ ] **Step 2: Manual QA checklist**

1. Translate mode (`G`): hover X/Y/Z arrows, plane squares, center disc — each highlights on mouse over, clears on leave.
2. Rotate mode (`R`): hover colored dials only (not trackball/outer ring).
3. Scale mode (`S`): hover box+stem axes, plane corners, center cube.
4. Start drag on handle — hover clears, dragged handle stays highlighted.
5. Move mouse over mesh behind gizmo — selection unchanged; releasing still picks mesh.
6. Orbit camera (right-drag) — no spurious hover stuck after release.

- [ ] **Step 3: Commit**

```bash
git add engine/src/runtime/function/render/render_system.cpp
git commit -m "feat: update gizmo hover from viewport mouse move"
```

---

## Task 5: Spec + docs

**Files:**
- Modify: `openspec/specs/transform-gizmo/spec.md` (via OpenSpec sync after propose)
- Modify: `docs/agents/render-pipeline.md`

- [ ] **Step 1: OpenSpec delta (if not done at start)**

Add requirement scenario:

```markdown
### Requirement: Gizmo handle hover highlight

When a transform gizmo mode is active and the user is not dragging, the handle under the cursor SHALL render with highlight color (`color_hi`).

#### Scenario: Hover highlights handle under cursor

- **WHEN** translate, rotate, or scale mode is active
- **AND** the user moves the mouse over a visible interactive handle
- **THEN** that handle renders with `color_hi` until the cursor leaves the handle

#### Scenario: Drag highlight takes precedence

- **WHEN** the user is dragging a gizmo handle
- **THEN** only the active dragged handle is highlighted
- **AND** hover highlight is suppressed until drag ends
```

- [ ] **Step 2: Add render-pipeline note**

Under transform gizmo input priority, add:

```markdown
**Hover:** `MouseMoved` in viewport updates `TransformGizmoController` hover via CPU analytic pick (same functions as click). Redraw only when hovered axis changes. Does not set `event.handled`.
```

- [ ] **Step 3: Run tests + validate**

```powershell
ctest --test-dir build/vs2026-debug -C Debug -R transform_gizmo_hover_test
openspec validate gizmo-handle-hover
```

- [ ] **Step 4: Commit**

```bash
git add openspec/ docs/agents/render-pipeline.md
git commit -m "docs: gizmo handle hover spec and render-pipeline note"
```

---

## Self-review

| Spec requirement | Task |
|------------------|------|
| Hover highlights handle under cursor | Tasks 2–4 |
| Drag highlight precedence | Task 2 (`clearHover` on press; `isHandleHighlighted` prefers active) |
| Reuse existing pick geometry | Task 1 `pickTransformGizmoHandle` |
| No mesh pick regression | Task 4 — no `event.handled` on hover |
| Blender `color_hi` visual | Task 3 — existing overlay path |

**Placeholder scan:** No TBD/TODO steps.

**Type consistency:** `ManipulatorAxis`, `TransformGizmoPickContext`, `Vec2` used consistently across tasks.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-04-gizmo-handle-hover.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
