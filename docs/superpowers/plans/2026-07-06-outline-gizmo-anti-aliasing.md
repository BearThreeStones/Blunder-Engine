# Outline + Transform Gizmo Anti-Aliasing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Visually smooth (anti-aliased) selection outline edges and transform gizmo polyline handles (rotate rings, scale annulus, ghost arc) without MSAA, matching Blender overlay smooth-wire quality at 1× viewport resolution.

**Architecture:** Selection outline already resolves via `outline_resolve.slang` `edgeCoverage()` (8-neighbor `smoothstep`). This plan **tunes and regression-tests** that kernel, then adds **screen-space polyline AA** to `transform_gizmo.slang` using a `strokeCoord` varying (same pattern as Blender `GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR` / navigate gizmo `smoothstep` disc edges). Shared CPU helpers mirror shader math for TDD. Transform gizmo draws in `ScreenOverlayPass` (no depth) — AA must be shader-based, not `OverlayAntiAliasing` (that pass only composites axis/wireframe line MRT).

**Tech Stack:** C++20, Slang (`outline_resolve.slang`, `transform_gizmo.slang`), Vulkan screen/offscreen passes, unit tests in `engine/src/tests/`, OpenSpec delta under `openspec/changes/`.

**Prerequisite:** `outline-smooth-resolve` spec archived (baseline smooth resolve landed). Transform gizmo polyline extrusion (`emitPolyline`, `line_width_px`) stable.

**Out of scope:** MSAA on offscreen target; `BLUNDER_EDITOR_OVERLAY_AA` line MRT path changes; navigate gizmo (already has disc `smoothstep`); solid mesh gizmo silhouettes (arrow cones, plane quads) — optional follow-up Task 6; theme-configurable AA width.

---

## Current state (gap analysis)

| Area | Today | Problem |
|------|-------|---------|
| Outline resolve | `edgeCoverage()` 8-neighbor binary test → `smoothstep(0.5, 3.0, weight)` | Stair-steps on shallow diagonals / `BLUNDER_EDITOR_RENDER_SCALE < 1`; no unit tests |
| Transform gizmo polylines | Screen-space extruded quads, hard `fragmentMain` alpha | Visible aliasing on rotate dials, outer ring, annulus, ghost arc |
| Transform gizmo solids | Arrow/plane/box tris, hard edges | Lower priority than rings (thicker geometry) |
| `overlay_aa.slang` | Composites `OverlayLineTargets` only | Does **not** run on transform gizmo (screen pass) |

**Blender reference:** `overlay_outline_detect_frag.glsl` (silhouette mask); `GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR` + `immUniform1f("lineWidth", …)` for gizmo rings (`transform_gizmo_3d.cc` dial draws).

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/overlay/outline_aa.h` | **New** — CPU mirror of outline edge coverage math |
| `engine/src/runtime/function/render/overlay/outline_aa.cpp` | **New** — `outlineEdgeCoverage()` tested without GPU |
| `engine/shaders/outline_resolve.slang` | Tune `edgeCoverage`; call shared constants |
| `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.h` | **New** — `polylineStrokeAlpha(strokeCoord, fw)` |
| `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.cpp` | **New** — CPU implementation |
| `engine/shaders/transform_gizmo.slang` | `strokeCoord` varying + fragment AA for polyline styles |
| `engine/src/tests/outline_aa_test.cpp` | **New** — outline coverage regression |
| `engine/src/tests/transform_gizmo_aa_test.cpp` | **New** — polyline stroke alpha regression |
| `engine/src/tests/CMakeLists.txt` | Register new tests |
| `openspec/changes/outline-gizmo-anti-aliasing/` | **New** — delta specs |
| `docs/agents/render-pipeline.md` | Document AA paths |

---

### Task 1: Outline edge coverage — CPU helper + tests (TDD)

**Files:**
- Create: `engine/src/runtime/function/render/overlay/outline_aa.h`
- Create: `engine/src/runtime/function/render/overlay/outline_aa.cpp`
- Create: `engine/src/tests/outline_aa_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: `engine/src/runtime/function/render/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Create `engine/src/tests/outline_aa_test.cpp`:

```cpp
#include <cstdio>
#include <cmath>
#include "runtime/function/render/overlay/outline_aa.h"

int main() {
  int failures = 0;
  auto expect_near = [&](const char* label, float a, float b) {
    if (std::fabs(a - b) > 1e-4f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };

  // Interior pixel — no edge neighbors differ
  expect_near("interior coverage 0",
              outlineEdgeCoverage(/*neighbor_edge_count=*/0), 0.0f);
  // Full ring of 8 edge neighbors (silhouette)
  expect_near("full silhouette coverage 1",
              outlineEdgeCoverage(8), 1.0f);
  // Partial diagonal edge
  const float partial = outlineEdgeCoverage(2);
  if (partial <= 0.0f || partial >= 1.0f) {
    std::fprintf(stderr, "FAIL partial edge in (0,1)\n");
    ++failures;
  }

  if (failures == 0) {
    std::printf("outline_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
```

Add CMake target (mirror `navigate_gizmo_style_test` pattern):

```cmake
add_executable(outline_aa_test outline_aa_test.cpp)
target_link_libraries(outline_aa_test PRIVATE engine_runtime)
add_test(NAME outline_aa_test COMMAND outline_aa_test)
set_tests_properties(outline_aa_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/engine/src/editor/Debug")
```

- [ ] **Step 2: Run test — verify RED**

```powershell
cmake --build build/vs2026-debug --target outline_aa_test --config Debug
Set-Location build\vs2026-debug\engine\src\editor\Debug
& "..\..\tests\Debug\outline_aa_test.exe"
```

Expected: FAIL — `outline_aa.h` not found

- [ ] **Step 3: Implement CPU helper**

`outline_aa.h`:

```cpp
#pragma once

namespace Blunder {

/// Mirrors outline_resolve.slang edgeCoverage() — neighbor_edge_count in [0,8].
float outlineEdgeCoverage(int neighbor_edge_count);

}  // namespace Blunder
```

`outline_aa.cpp` (extract current shader logic):

```cpp
#include "runtime/function/render/overlay/outline_aa.h"
#include <algorithm>

namespace Blunder {

float outlineEdgeCoverage(const int neighbor_edge_count) {
  const float weight = static_cast<float>(std::clamp(neighbor_edge_count, 0, 8));
  return std::max(0.0f, std::min(1.0f, (weight - 0.5f) / 2.5f)); // smoothstep(0.5, 3.0, w)
}

}  // namespace Blunder
```

Add `outline_aa.cpp` to `engine/src/runtime/function/render/CMakeLists.txt`.

- [ ] **Step 4: Run test — verify GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/function/render/overlay/outline_aa.* \
        engine/src/tests/outline_aa_test.cpp engine/src/tests/CMakeLists.txt \
        engine/src/runtime/function/render/CMakeLists.txt
git commit -m "test: outline edge coverage CPU helper"
```

---

### Task 2: Outline AA quality — wider kernel + shader sync

**Files:**
- Modify: `engine/shaders/outline_resolve.slang`
- Modify: `engine/src/runtime/function/render/overlay/outline_aa.h`
- Modify: `engine/src/runtime/function/render/overlay/outline_aa.cpp`
- Modify: `engine/src/tests/outline_aa_test.cpp`

- [ ] **Step 1: Write failing test for improved diagonal response**

Append to `outline_aa_test.cpp`:

```cpp
  // 3 neighbors — typical shallow diagonal; upgraded kernel should be > 0.25
  const float diag = outlineEdgeCoverage(3);
  if (diag < 0.25f) {
    std::fprintf(stderr, "FAIL diagonal edge too faint (got %f)\n", diag);
    ++failures;
  }
```

- [ ] **Step 2: Run test — verify RED** (with old smoothstep, weight=3 → (3-0.5)/2.5 = 1.0 actually passes)

Adjust test to target **2 neighbors** after kernel change:

```cpp
  const float two = outlineEdgeCoverage(2);
  expect_near("two-neighbor diagonal coverage", two, 0.35f); // tune after impl
```

Run RED with expected value higher than current `(2-0.5)/2.5 = 0.6` — instead test **lower edge** softness:

Add test for constants exported:

```cpp
  expect_near("smooth min", kOutlineEdgeSmoothMin, 0.25f);
  expect_near("smooth max", kOutlineEdgeSmoothMax, 2.5f);
```

- [ ] **Step 3: Widen smoothstep + diagonal weights**

In `outline_aa.h`:

```cpp
constexpr float kOutlineEdgeSmoothMin = 0.25f;
constexpr float kOutlineEdgeSmoothMax = 2.5f;
```

Update `outlineEdgeCoverage`:

```cpp
float outlineEdgeCoverage(const int neighbor_edge_count) {
  const float w = static_cast<float>(std::clamp(neighbor_edge_count, 0, 8));
  const float t = (w - kOutlineEdgeSmoothMin) /
                  std::max(kOutlineEdgeSmoothMax - kOutlineEdgeSmoothMin, 1e-4f);
  return std::clamp(t, 0.0f, 1.0f);
}
```

In `outline_resolve.slang`, replace hardcoded `smoothstep(0.5, 3.0, weight)`:

```slang
static const float kEdgeSmoothMin = 0.25;
static const float kEdgeSmoothMax = 2.5;

float edgeCoverage(uint refId, int2 pixel)
{
    // ... existing neighbor loop ...
    float t = (weight - kEdgeSmoothMin) / max(kEdgeSmoothMax - kEdgeSmoothMin, 1e-4);
    return saturate(t);
}
```

Optional: weight diagonal neighbors `0.707` instead of `1.0` in the loop for smoother corners.

- [ ] **Step 4: Run `outline_aa_test` — verify GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/shaders/outline_resolve.slang \
        engine/src/runtime/function/render/overlay/outline_aa.* \
        engine/src/tests/outline_aa_test.cpp
git commit -m "feat: widen selection outline anti-alias kernel"
```

---

### Task 3: Transform gizmo polyline AA — CPU helper (TDD)

**Files:**
- Create: `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.h`
- Create: `engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.cpp`
- Create: `engine/src/tests/transform_gizmo_aa_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: `engine/src/runtime/function/render/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

```cpp
#include <cstdio>
#include <cmath>
#include "runtime/function/render/gizmo/gizmo_polyline_aa.h"

int main() {
  int failures = 0;
  auto expect_near = [&](const char* label, float a, float b) {
    if (std::fabs(a - b) > 1e-3f) {
      std::fprintf(stderr, "FAIL %s\n", label);
      ++failures;
    }
  };

  expect_near("stroke center full alpha",
              polylineStrokeAlpha(0.0f, 0.1f), 1.0f);
  expect_near("stroke edge zero alpha",
              polylineStrokeAlpha(1.0f, 0.1f), 0.0f);
  const float mid = polylineStrokeAlpha(0.8f, 0.1f);
  if (mid <= 0.0f || mid >= 1.0f) {
    std::fprintf(stderr, "FAIL mid stroke partial alpha\n");
    ++failures;
  }

  if (failures == 0) {
    std::printf("transform_gizmo_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
```

- [ ] **Step 2: Run test — verify RED**

- [ ] **Step 3: Implement helper**

`gizmo_polyline_aa.h`:

```cpp
#pragma once

namespace Blunder {

/// stroke_coord: 0 = center of polyline quad, 1 = outer edge; fw = filter width (~fwidth).
float polylineStrokeAlpha(float stroke_coord, float fw);

}  // namespace Blunder
```

`gizmo_polyline_aa.cpp`:

```cpp
#include "runtime/function/render/gizmo/gizmo_polyline_aa.h"
#include <algorithm>

namespace Blunder {

float polylineStrokeAlpha(const float stroke_coord, const float fw) {
  const float dist = std::clamp(std::fabs(stroke_coord), 0.0f, 1.0f);
  const float aa = std::max(fw * 1.5f, 1e-4f);
  const float t = (dist - (1.0f - aa)) / aa;
  return 1.0f - std::clamp(t, 0.0f, 1.0f);
}

}  // namespace Blunder
```

- [ ] **Step 4: Run test — verify GREEN**

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/function/render/gizmo/gizmo_polyline_aa.* \
        engine/src/tests/transform_gizmo_aa_test.cpp engine/src/tests/CMakeLists.txt \
        engine/src/runtime/function/render/CMakeLists.txt
git commit -m "test: transform gizmo polyline AA helper"
```

---

### Task 4: Transform gizmo shader — `strokeCoord` varying + fragment AA

**Files:**
- Modify: `engine/shaders/transform_gizmo.slang`

- [ ] **Step 1: Extend vertex output**

```slang
struct VertexOutput {
    float4 sv_position : SV_Position;
    [[vk::location(0)]] float3 worldNormal;
    [[vk::location(1)]] float4 color;
    [[vk::location(2)]] float3 worldPos;
    [[vk::location(3)]] float strokeCoord; // |coord|<=1 polyline; >1 solid (skip AA)
};
```

- [ ] **Step 2: Set strokeCoord in emitters**

In `emitV`:

```slang
o.strokeCoord = 2.0; // solid mesh — no polyline AA
```

In `emitPolyline`, after computing `ap`:

```slang
o.strokeCoord = ap.y; // -1..+1 across line width (interpolates in fragment)
```

- [ ] **Step 3: Fragment AA**

Replace `fragmentMain` body:

```slang
[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    if (input.color.a < 0.001) {
        discard;
    }
    if (ubo.clip_enabled > 0.5) {
        float clip_dist = dot(ubo.clip_plane.xyz, input.worldPos) + ubo.clip_plane.w;
        if (clip_dist < 0.0) {
            discard;
        }
    }
    float4 color = input.color;
    if (abs(input.strokeCoord) <= 1.0) {
        float dist = abs(input.strokeCoord);
        float fw = fwidth(dist);
        float aa = max(fw * 1.5, 1e-4);
        float t = saturate((dist - (1.0 - aa)) / aa);
        color.a *= (1.0 - t);
    }
    return color;
}
```

- [ ] **Step 4: Build `engine_runtime`**

```powershell
cmake --build build/vs2026-debug --target engine_runtime --config Debug
```

Expected: success (Slang recompiles `transform_gizmo.slang`)

- [ ] **Step 5: Run `transform_gizmo_aa_test` + `transform_gizmo_hover_test`**

Expected: all passed (shader change does not break CPU tests)

- [ ] **Step 6: Commit**

```bash
git add engine/shaders/transform_gizmo.slang
git commit -m "feat: anti-alias transform gizmo polyline handles in shader"
```

---

### Task 5: OpenSpec + docs + manual QA

**Files:**
- Create: `openspec/changes/outline-gizmo-anti-aliasing/proposal.md`, `design.md`, `tasks.md`, `.openspec.yaml`
- Create: `openspec/changes/outline-gizmo-anti-aliasing/specs/outline-smooth-resolve/spec.md` (delta)
- Create: `openspec/changes/outline-gizmo-anti-aliasing/specs/transform-gizmo/spec.md` (delta)
- Modify: `docs/agents/render-pipeline.md`

- [ ] **Step 1: Delta spec — outline**

```markdown
## MODIFIED Requirements

### Requirement: Smooth anti-aliased outline edges

The outline resolve pass SHALL use a smoothstep edge kernel with `kOutlineEdgeSmoothMin = 0.25` and `kOutlineEdgeSmoothMax = 2.5` on an 8-neighbor silhouette mask.

#### Scenario: Shallow diagonal edge coverage

- **WHEN** a silhouette pixel has 2–3 edge neighbors
- **THEN** outline coverage SHALL be partial alpha (not binary 0 or 1 only)
```

- [ ] **Step 2: Delta spec — transform gizmo**

```markdown
## ADDED Requirements

### Requirement: Transform gizmo polyline anti-aliasing

Rotate dial, outer ring, annulus, and ghost arc polylines SHALL render with screen-space edge softening (`strokeCoord` + `fwidth` smoothstep), matching Blender smooth polyline overlay quality.

#### Scenario: Rotate ring edge softness

- **WHEN** rotate mode is active and the viewport shows axis dials
- **THEN** dial ring edges SHALL show partial-alpha pixels (no 1-pixel aliased crawl)
```

- [ ] **Step 3: Update `render-pipeline.md`**

Add under selection outline section:

```markdown
**Outline AA:** `outline_resolve.slang` `edgeCoverage()` — 8-neighbor silhouette mask, smoothstep `[0.25, 2.5]`. CPU mirror: `outline_aa.cpp`.

**Transform gizmo AA:** Polyline styles in `transform_gizmo.slang` use `strokeCoord` varying + fragment `fwidth` softening. CPU mirror: `gizmo_polyline_aa.cpp`. Screen pass (not `overlay_aa.slang`).
```

- [ ] **Step 4: Validate**

```powershell
openspec validate outline-gizmo-anti-aliasing
```

- [ ] **Step 5: Manual QA checklist**

| Check | Expected |
|-------|----------|
| Select cube, static camera | Orange outline smooth on diagonal edges |
| `BLUNDER_EDITOR_RENDER_SCALE=0.75` | Outline still readable, not broken |
| Rotate gizmo (R) | Dial rings smooth, not jaggy |
| Scale gizmo (S) | Outer annulus smooth |
| Translate arrows | Acceptable (solids — no regression) |

- [ ] **Step 6: Commit**

```bash
git add openspec/changes/outline-gizmo-anti-aliasing/ docs/agents/render-pipeline.md
git commit -m "docs: outline and transform gizmo anti-aliasing spec"
```

---

### Task 6 (optional): Solid gizmo mesh edge softening

**Files:**
- Modify: `engine/shaders/transform_gizmo.slang`

Only pursue if manual QA shows arrow/plane edges still distract.

- [ ] **Step 1: Add `derivate-based` alpha on style 0/1/5/9 using `fwidth` on clip-space depth or screen-space silhouette**

YAGNI until Task 5 manual QA fails on solids.

---

## Self-review

| Requirement | Task |
|-------------|------|
| Outline smooth edges (spec) | Task 1–2 |
| Outline regression tests | Task 1 |
| Transform gizmo polyline AA | Task 3–4 |
| Polyline AA tests | Task 3 |
| Docs + OpenSpec | Task 5 |
| Solid mesh AA | Task 6 (optional) |

**Placeholder scan:** None.

**Type consistency:** `outlineEdgeCoverage(int)` and `polylineStrokeAlpha(float,float)` used in tests and mirrored in shaders; `strokeCoord > 1` sentinel for solids.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-06-outline-gizmo-anti-aliasing.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task, spec + code review between tasks

**2. Inline Execution** — execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
