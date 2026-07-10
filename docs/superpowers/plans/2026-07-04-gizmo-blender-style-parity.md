# Gizmo Blender Style Parity — Transform + Navigate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make transform gizmo and navigate (orientation) gizmo **visual style** match Blender 4.x default theme — colors, alpha, depth fade, hover feedback, and layout constants — using local Blender source at `e:\Dev\blender\` as the reference of record.

**Architecture:** Blender splits style into (1) per-frame color pairs from `gizmo_get_axis_color` / theme tokens, selected at draw via `gizmo_color_get`, and (2) navigate-specific depth mixing against the 3D view background in `VIEW3D_GT_navigate_rotate`. Blunder already mirrors (1) for transform handles via `gizmoAxisColor` + `gizmoColorGet` (landed uncommitted in `gizmo-handle-hover`). This plan commits that work, fixes remaining theme-token gaps, then introduces `navigate_gizmo_style.{h,cpp}` as the single CPU style source feeding `navigate_gizmo_overlay.cpp` and `navigate_gizmo.slang` uniforms. Navigate hover mirrors Blender `WM_GIZMO_STATE_HIGHLIGHT` (backdrop) + per-part highlight.

**Tech Stack:** C++20, GLSL/Slang (`navigate_gizmo.slang`, `transform_gizmo.slang`), Vulkan screen overlay pass, unit tests in `engine/src/tests/`, OpenSpec under `openspec/changes/`.

**Prerequisite:** Uncommitted `gizmo-handle-hover` work (hover pick, `gizmoColorGet`, `gizmoHandleUsesHoverColorHi`, `transform_gizmo_hover_test.cpp`) — do **not** re-implement; commit in Task 1.

**Blender reference files (read-only):**

| Topic | Blender path |
|-------|----------------|
| Transform axis colors | `source/blender/editors/transform/transform_gizmo_3d.cc` L96–105, L325–395 |
| `gizmo_color_get` | `source/blender/editors/gizmo_library/gizmo_library_utils.cc` L160–168 |
| Default theme RGBA | `release/datafiles/userdef/userdef_default_theme.c` L262–269 |
| Navigate rotate draw | `source/blender/editors/space_view3d/view3d_gizmo_navigate_type.cc` |
| Navigate layout/setup | `source/blender/editors/space_view3d/view3d_gizmo_navigate.cc` L36–46, L179–183, L243–246 |
| Navigate gizmo size default | `source/blender/makesdna/DNA_userdef_types.h` (`gizmo_size_navigate_v3d = 80`) |

**Out of scope:** Slint toolbar mini-buttons (pan/zoom/camera icons beside navigate gizmo); runtime theme editor; `U.gizmo_size` preference wiring; transform gizmo line-width change on hover; `rot_t` show-on-hover visibility (`WM_GIZMO_DRAW_HOVER` decor visibility).

---

## Gap analysis (current vs Blender)

| Area | Blender | Blunder today | Task |
|------|---------|---------------|------|
| Axis RGB (default theme) | X `#FF3352`, Y `#8BDC00`, Z `#2890FF` | `coordinate_system.h` approximations | Task 2 |
| `TH_GIZMO_VIEW_ALIGN` | White `(1,1,1)` default | Gray `0.78` in `transform_gizmo_types.cpp` | Task 2 |
| Transform alpha / fade | `0.6` / `1.0` × `alpha_fac`; rot rings no fade | Implemented (uncommitted) | Task 1 |
| Transform hover `color_hi` | `gizmo_color_get` | Implemented (uncommitted) | Task 1 |
| Decor no `color_hi` | `WM_GIZMO_DRAW_HOVER` axes | `gizmoHandleUsesHoverColorHi` | Task 1 |
| Navigate size | `80px` diameter default | `110px` hardcoded | Task 6 |
| Navigate depth fade | `interp(view_bg, axis, f(depth))` | Flat `0.95` alpha | Task 4 |
| Navigate neg axis | `interp(view_bg, axis, 0.25)` + aligned special case | `0.5 × alpha` flat | Task 4 |
| Navigate hover backdrop | `color_hi` roundbox when highlighted | Always static `bg_color` | Task 5 |
| Navigate per-axis highlight | White label + ring emphasis | No hover state | Task 5 |
| Navigate line/ball metrics | `AXIS_HANDLE_SIZE 0.20`, widths from `gizmo_size/40` | `NavigateGizmoMetrics` literals | Task 3 |

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/core/math/coordinate_system.h` | Blender default `TH_AXIS_*` RGB constants |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_types.{h,cpp}` | Transform color pair + `gizmoColorGet` (existing) |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` | Draw paths via `gizmoColorGet` (existing) |
| `engine/src/runtime/function/render/overlay/navigate_gizmo_style.{h,cpp}` | **New** — depth fade, hover colors, metrics from Blender formulas |
| `engine/src/runtime/function/render/overlay/navigate_gizmo_shared.h` | Geometry constants (sync with style module) |
| `engine/src/runtime/function/render/overlay/navigate_gizmo_layout.h` | Size = Blender `gizmo_size_navigate_v3d` + offset |
| `engine/src/runtime/function/render/overlay/navigate_gizmo_overlay.{h,cpp}` | Build UBO from style module; hover on `MouseMoved` |
| `engine/shaders/navigate_gizmo.slang` | Per-vertex colors from UBO; optional depth uniforms |
| `engine/src/runtime/function/render/render_system.cpp` | Navigate hover `MouseMoved` wiring |
| `engine/src/tests/transform_gizmo_hover_test.cpp` | Transform style regression (existing) |
| `engine/src/tests/navigate_gizmo_style_test.cpp` | **New** — navigate color interp tests |
| `engine/src/tests/CMakeLists.txt` | Register navigate style test |
| `openspec/changes/gizmo-handle-hover/` | Archive after Task 1 + manual QA |
| `openspec/changes/gizmo-blender-style-parity/` | **New** — navigate + theme requirements |
| `docs/agents/render-pipeline.md` | Document both gizmo style paths |

---

### Task 1: Commit transform gizmo hover + style (landed work)

**Files:**
- Stage: all `gizmo-handle-hover` files (see git status)
- Test: `engine/src/tests/transform_gizmo_hover_test.cpp`

- [ ] **Step 1: Verify tests pass**

```powershell
cmake --build build/vs2026-debug --target transform_gizmo_hover_test --config Debug
Set-Location build\vs2026-debug\engine\src\editor\Debug
& "..\..\tests\Debug\transform_gizmo_hover_test.exe"
```

Expected: `transform_gizmo_hover_test: all passed`

- [ ] **Step 2: Commit gizmo-handle-hover**

```bash
git add engine/src/runtime/function/render/gizmo/transform_gizmo_*.cpp \
        engine/src/runtime/function/render/gizmo/transform_gizmo_*.h \
        engine/src/runtime/function/render/gizmo/gizmo_math.* \
        engine/src/runtime/function/render/render_system.cpp \
        engine/src/tests/transform_gizmo_hover_test.cpp \
        engine/src/tests/CMakeLists.txt \
        openspec/changes/gizmo-handle-hover/ \
        docs/agents/render-pipeline.md
git commit -m "feat: transform gizmo handle hover with Blender color_hi parity"
```

---

### Task 2: Blender default theme tokens (transform colors)

**Files:**
- Modify: `engine/src/runtime/core/math/coordinate_system.h`
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_types.cpp`
- Modify: `engine/src/tests/transform_gizmo_hover_test.cpp`
- Test: `engine/src/tests/transform_gizmo_hover_test.cpp`

- [ ] **Step 1: Write failing test for Blender default axis RGB**

Append to `transform_gizmo_hover_test.cpp`:

```cpp
  {
    const float idot[3] = {1.0f, 1.0f, 1.0f};
    const GizmoAxisColor x = gizmoAxisColor(ManipulatorAxis::trans_x, idot);
    expect_true("blender theme X red channel",
                std::fabs(x.color.r - (255.0f / 255.0f)) < 1e-4f);
    expect_true("blender theme X green ~0.2",
                std::fabs(x.color.g - (51.0f / 255.0f)) < 0.02f);
    const GizmoAxisColor center =
        gizmoAxisColor(ManipulatorAxis::trans_c, idot);
    expect_true("blender TH_GIZMO_VIEW_ALIGN is white",
                center.color.r > 0.99f && center.color.g > 0.99f &&
                    center.color.b > 0.99f);
  }
```

- [ ] **Step 2: Run test — verify RED**

```powershell
cmake --build build/vs2026-debug --target transform_gizmo_hover_test --config Debug
Set-Location build\vs2026-debug\engine\src\editor\Debug
& "..\..\tests\Debug\transform_gizmo_hover_test.exe"
```

Expected: FAIL on theme X / view-align assertions

- [ ] **Step 3: Update theme constants**

In `coordinate_system.h`, replace axis colors with Blender `userdef_default_theme.c` values:

```cpp
inline const Vec3 kAxisColorPositiveX{255.0f / 255.0f, 51.0f / 255.0f, 82.0f / 255.0f};
inline const Vec3 kAxisColorPositiveY{139.0f / 255.0f, 220.0f / 255.0f, 0.0f / 255.0f};
inline const Vec3 kAxisColorPositiveZ{40.0f / 255.0f, 144.0f / 255.0f, 255.0f / 255.0f};
```

In `transform_gizmo_types.cpp`, change `kGizmoViewAlignColor` to white:

```cpp
const glm::vec3 kGizmoViewAlignColor{1.0f, 1.0f, 1.0f};
```

- [ ] **Step 4: Run test — verify GREEN**

Expected: all passed

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/core/math/coordinate_system.h \
        engine/src/runtime/function/render/gizmo/transform_gizmo_types.cpp \
        engine/src/tests/transform_gizmo_hover_test.cpp
git commit -m "fix: align gizmo theme colors with Blender default theme"
```

---

### Task 3: Navigate gizmo metrics from Blender formulas

**Files:**
- Create: `engine/src/runtime/function/render/overlay/navigate_gizmo_style.h`
- Modify: `engine/src/runtime/function/render/overlay/navigate_gizmo_shared.h`
- Modify: `engine/src/runtime/function/render/overlay/navigate_gizmo_layout.h`
- Create: `engine/src/tests/navigate_gizmo_style_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test for default navigate diameter**

Create `engine/src/tests/navigate_gizmo_style_test.cpp`:

```cpp
#include <cstdio>
#include <cmath>
#include "runtime/function/render/overlay/navigate_gizmo_style.h"
#include "runtime/function/render/overlay/navigate_gizmo_layout.h"

int main() {
  using namespace Blunder;
  int failures = 0;
  auto expect_near = [&](const char* label, float a, float b) {
    if (std::fabs(a - b) > 1e-3f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };
  expect_near("blender default navigate diameter 80",
              navigateGizmoDiameterPx(/*ui_scale=*/1.0f), 80.0f);
  const auto layout = computeNavigateGizmoLayout(1920, 1080);
  expect_near("layout size matches diameter", layout.gizmo_size, 80.0f);
  expect_near("blender AXIS_HANDLE_SIZE ratio",
              NavigateGizmoMetrics::kBallRadius /
                  NavigateGizmoMetrics::kBgRadius,
              0.20f);
  if (failures == 0) {
    std::printf("navigate_gizmo_style_test: all passed\n");
    return 0;
  }
  return 1;
}
```

Add to `engine/src/tests/CMakeLists.txt` (mirror `transform_gizmo_hover_test` pattern):

```cmake
add_executable(navigate_gizmo_style_test navigate_gizmo_style_test.cpp)
target_link_libraries(navigate_gizmo_style_test PRIVATE engine_runtime)
add_test(NAME navigate_gizmo_style_test COMMAND navigate_gizmo_style_test)
set_tests_properties(navigate_gizmo_style_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/engine/src/editor/Debug")
```

- [ ] **Step 2: Run test — verify RED**

```powershell
cmake --build build/vs2026-debug --target navigate_gizmo_style_test --config Debug
Set-Location build\vs2026-debug\engine\src\editor\Debug
& "..\..\tests\Debug\navigate_gizmo_style_test.exe"
```

Expected: FAIL — `navigate_gizmo_style.h` missing; layout still 110

- [ ] **Step 3: Implement style header + update layout**

`navigate_gizmo_style.h`:

```cpp
#pragma once

namespace Blunder {

/// Blender DNA_userdef_types.h gizmo_size_navigate_v3d default.
constexpr float kBlenderNavigateGizmoDiameterPx = 80.0f;
constexpr float kBlenderNavigateGizmoOffsetPx = 10.0f;

inline float navigateGizmoDiameterPx(float ui_scale) {
  return kBlenderNavigateGizmoDiameterPx * ui_scale;
}

}  // namespace Blunder
```

In `navigate_gizmo_layout.h`, replace `kNavigateGizmoBaseSize = 110.0f` with:

```cpp
constexpr float kNavigateGizmoBaseSize = kBlenderNavigateGizmoDiameterPx;
```

(include `navigate_gizmo_style.h`)

In `navigate_gizmo_shared.h`, set metrics to match Blender `view3d_gizmo_navigate_type.cc`:

```cpp
// WIDGET_RADIUS = diameter/2; arm ends at (1 - AXIS_HANDLE_SIZE) = 0.80
static constexpr float kBgRadius = 1.0f;
static constexpr float kArmLength = 0.80f;          // was 0.68
static constexpr float kBallRadius = 0.20f;         // AXIS_HANDLE_SIZE
static constexpr float kNegBallRadius = 0.12f;      // keep until Task 4 refines
static constexpr float kCenterRadius = 0.0f;        // Blender has no center dot
static constexpr float kArmHalfWidth = 0.025f;      // ~(diameter/40)/diameter
```

- [ ] **Step 4: Run test — verify GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/function/render/overlay/navigate_gizmo_style.h \
        engine/src/runtime/function/render/overlay/navigate_gizmo_shared.h \
        engine/src/runtime/function/render/overlay/navigate_gizmo_layout.h \
        engine/src/tests/navigate_gizmo_style_test.cpp \
        engine/src/tests/CMakeLists.txt
git commit -m "feat: navigate gizmo metrics from Blender VIEW3D_GT_navigate_rotate"
```

---

### Task 4: Navigate depth-based axis colors (Blender interp)

**Files:**
- Modify: `engine/src/runtime/function/render/overlay/navigate_gizmo_style.h`
- Create: `engine/src/runtime/function/render/overlay/navigate_gizmo_style.cpp`
- Modify: `engine/src/runtime/function/render/overlay/navigate_gizmo_overlay.cpp`
- Modify: `engine/shaders/navigate_gizmo.slang`
- Modify: `engine/src/tests/navigate_gizmo_style_test.cpp`

- [ ] **Step 1: Write failing tests for depth fade**

Append to `navigate_gizmo_style_test.cpp`:

```cpp
  {
    const glm::vec4 view_bg{0.12f, 0.12f, 0.14f, 1.0f};
    const glm::vec3 axis_rgb = kAxisColorPositiveX;
    // Blender: fading_color = interp(view, axis, ((depth+1)*0.25)+0.5) at depth=1
    const glm::vec4 faded = navigateAxisFadingColor(view_bg, axis_rgb, 1.0f);
    expect_near("front axis brighter than view bg", faded.r, 0.55f); // tune to formula
    const glm::vec4 neg = navigateAxisNegativeColor(view_bg, axis_rgb, -0.5f, false, false);
    if (neg.a <= 0.0f) { std::fprintf(stderr, "FAIL neg axis visible\n"); ++failures; }
  }
```

- [ ] **Step 2: Run test — verify RED**

- [ ] **Step 3: Implement CPU color helpers**

`navigate_gizmo_style.cpp` (mirror Blender L182–188, L219–230):

```cpp
glm::vec4 navigateAxisFadingColor(const glm::vec4& view_bg, const glm::vec3& axis_rgb,
                                  float depth) {
  const float t = (depth + 1.0f) * 0.25f + 0.5f;
  glm::vec4 axis{axis_rgb, 1.0f};
  return glm::mix(view_bg, axis, t);
}

glm::vec4 navigateAxisMiddleColor(const glm::vec4& view_bg, const glm::vec3& axis_rgb) {
  glm::vec4 axis{axis_rgb, 1.0f};
  return glm::mix(view_bg, axis, 0.75f);
}

glm::vec4 navigateAxisNegativeColor(const glm::vec4& view_bg, const glm::vec3& axis_rgb,
                                    float depth, bool is_aligned_front, bool /*is_behind*/) {
  glm::vec4 out{};
  if (is_aligned_front) {
    const glm::vec4 white{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 axis{axis_rgb, 1.0f};
    out = glm::mix(white, axis, 0.5f);
    out.a = std::min(depth + 1.0f, 1.0f);
  } else {
    glm::vec4 axis{axis_rgb, 1.0f};
    out = glm::mix(view_bg, axis, 0.25f);
    out.a = std::min(depth + 1.0f, 1.0f);
  }
  return out;
}
```

Update `navigate_gizmo_overlay.cpp` to pass `view_bg` from scene clear color (read from overlay/grid background or hardcode Blender default viewport gray `{0.22, 0.22, 0.22, 1}` until wired).

Extend `GizmoUniformData` / shader: per sorted slot pass precomputed `fadingColor` and `middleColor` for arms; balls use fading/negative helpers on CPU instead of flat `axisColor.w = 0.95`.

- [ ] **Step 4: Run tests + visual check — verify GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/function/render/overlay/navigate_gizmo_style.* \
        engine/src/runtime/function/render/overlay/navigate_gizmo_overlay.cpp \
        engine/shaders/navigate_gizmo.slang \
        engine/src/tests/navigate_gizmo_style_test.cpp
git commit -m "feat: navigate gizmo depth fade colors match Blender"
```

---

### Task 5: Navigate gizmo hover (backdrop + per-axis highlight)

**Files:**
- Modify: `engine/src/runtime/function/render/overlay/navigate_gizmo_overlay.h`
- Modify: `engine/src/runtime/function/render/overlay/navigate_gizmo_overlay.cpp`
- Modify: `engine/src/runtime/function/render/render_system.cpp`
- Modify: `engine/shaders/navigate_gizmo.slang`
- Modify: `engine/src/tests/navigate_gizmo_style_test.cpp`

- [ ] **Step 1: Write failing test for hover backdrop color**

```cpp
  {
    const glm::vec4 backdrop = navigateGizmoHighlightBackdropColor();
    expect_near("hover backdrop gray 0.5", backdrop.r, 0.5f);
    expect_near("hover backdrop alpha 0.5", backdrop.a, 0.5f);
  }
```

Matches Blender setup `view3d_gizmo_navigate.cc` L179–182: `color_hi = 0.5`, alpha `0.5`.

- [ ] **Step 2: Run test — verify RED**

- [ ] **Step 3: Implement hover state**

In `NavigateGizmoOverlay`:

```cpp
std::optional<int> m_highlight_endpoint;  // 0..5, nullopt = none
bool m_gizmo_active = false;            // cursor inside widget radius

bool updateHoverFromPointer(const Vec2& window_pos, const EditorCamera& camera);
void clearHover();
```

`updateHoverFromPointer`: if inside navigate gizmo rect → `m_gizmo_active = true`; run existing `hitTestNavigateGizmoAxis` for `m_highlight_endpoint`; return true if state changed.

In `record_gizmo_draw`: when `m_gizmo_active`, set `bg_color` to `navigateGizmoHighlightBackdropColor()` instead of static dark disc (Blender L145–157).

In shader: when endpoint matches highlight uniform, force letter alpha `1.0` and white text (Blender L271–275).

Wire `RenderSystem::onEvent` `MouseMoved` (after transform gizmo hover block):

```cpp
if (m_overlay_system && m_editor_camera && !event.handled) {
  bool nav_hover_changed = false;
    // ... viewport bounds check ...
    nav_hover_changed = m_overlay_system->navigate_gizmo().updateHoverFromPointer(pos, *m_editor_camera);
  if (nav_hover_changed) requestViewportRedraw();
}
```

- [ ] **Step 4: Run tests — verify GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/function/render/overlay/navigate_gizmo_overlay.* \
        engine/src/runtime/function/render/render_system.cpp \
        engine/shaders/navigate_gizmo.slang \
        engine/src/tests/navigate_gizmo_style_test.cpp
git commit -m "feat: navigate gizmo hover backdrop and axis highlight"
```

---

### Task 6: OpenSpec, docs, manual QA, archive

**Files:**
- Create: `openspec/changes/gizmo-blender-style-parity/proposal.md`, `design.md`, `tasks.md`, `specs/transform-gizmo/spec.md`, `specs/navigate-gizmo/spec.md`
- Modify: `docs/agents/render-pipeline.md`
- Modify: `openspec/changes/gizmo-handle-hover/tasks.md` (mark manual QA done after verification)

- [ ] **Step 1: Add navigate gizmo delta spec scenario**

```markdown
### Requirement: Navigate gizmo Blender visual parity

The viewport navigate orientation gizmo SHALL match Blender `VIEW3D_GT_navigate_rotate` default theme: axis RGB from `TH_AXIS_*`, depth-based fade against view background, diameter 80px at UI scale 1.0, offset 10px from top-right.

#### Scenario: Hover shows highlight backdrop

- **WHEN** the cursor is inside the navigate gizmo bounds
- **THEN** a circular backdrop using `color_hi` (gray 0.5, alpha 0.5) is drawn
- **AND** the axis under the cursor shows emphasized label styling
```

- [ ] **Step 2: Update render-pipeline.md** — add navigate style section referencing `navigate_gizmo_style` and Blender files.

- [ ] **Step 3: Validate**

```powershell
openspec validate gizmo-blender-style-parity
openspec validate gizmo-handle-hover
```

- [ ] **Step 4: Manual side-by-side QA**

| Check | Blender | Blunder |
|-------|---------|---------|
| Transform idle X alpha | ~0.6 | ~0.6 |
| Transform hover X alpha | 1.0 | 1.0 |
| Transform center handle | white view-align | white |
| Navigate diameter | 80px | 80px |
| Navigate rear axis dimmed | yes | yes |
| Navigate hover backdrop | gray halo | gray halo |
| Navigate axis letters | X/Y/Z bold | X/Y/Z |

- [ ] **Step 5: Archive `gizmo-handle-hover`**

```powershell
/opsx:archive gizmo-handle-hover
```

- [ ] **Step 6: Commit**

```bash
git add openspec/changes/gizmo-blender-style-parity/ docs/agents/render-pipeline.md
git commit -m "docs: gizmo Blender style parity spec and render pipeline notes"
```

---

## Self-review

| Requirement | Task |
|-------------|------|
| Transform `gizmo_get_axis_color` alpha | Task 1 (done) |
| Transform `gizmo_color_get` hover | Task 1 (done) |
| Decor axes no `color_hi` | Task 1 (done) |
| Blender default `TH_AXIS_*` RGB | Task 2 |
| `TH_GIZMO_VIEW_ALIGN` white | Task 2 |
| Navigate diameter 80 / offset 10 | Task 3 |
| Navigate `AXIS_HANDLE_SIZE` 0.20 | Task 3 |
| Navigate depth fade colors | Task 4 |
| Navigate hover backdrop + axis hi | Task 5 |
| OpenSpec + manual QA | Task 6 |

**Placeholder scan:** None.

**Type consistency:** `navigateGizmoDiameterPx`, `navigateAxisFadingColor`, `navigateGizmoHighlightBackdropColor` defined in Task 3–5 before use in overlay/shader.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-04-gizmo-blender-style-parity.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task, spec + code review between tasks

**2. Inline Execution** — execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
