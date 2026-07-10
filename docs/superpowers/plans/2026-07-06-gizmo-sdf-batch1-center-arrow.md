# Gizmo SDF Batch 1 — Center Disc + Arrow Stem Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate translate **center disc** (`GizmoDrawStyle::center`) and **arrow stem** (`GizmoDrawStyle::arrow` stem) to the same local 2D SDF + Jacobian AA path used by rotate rings (`vsSdfRingQuad` / `ringLineAlpha`), eliminating polygon faceting and inconsistent polyline AA.

**Architecture:** Add CPU mirrors in `gizmo_sdf_aa.{h,cpp}` for TDD, then extend `transform_gizmo.slang` with `kStrokeSdfSegment = -4.0`, `segmentLineAlpha()`, and `vsSdfSegmentQuad()`. Center style switches from 12-sided fan (`vsCenter`, 72 verts) to one SDF disc quad (6 verts). Arrow stem switches from screen-space `emitPolyline` to local XZ segment SDF quad; cone mesh unchanged. Picking stays analytic (`transform_gizmo_pick.cpp`) — no pick changes.

**Tech Stack:** C++20, Slang (`transform_gizmo.slang`), Vulkan screen overlay pass, unit tests in `engine/src/tests/`, MSVC build `build/vs2026-debug`.

**Prerequisite:** Rotate ring SDF path stable (`ringLineAlpha`, `discFillAlpha`, `vsSdfRingQuad`). White outer ring uses `pad_scale = 2.5`.

**Out of scope (Batch 2):** translate plane borders, scale stem SDF, removing dead `emitPolyline` fragment path, OpenSpec archive.

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.h` | **New** — CPU `sdSegment2d`, `discFillAlpha`, `segmentLineAlpha` |
| `engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.cpp` | **New** — implementations mirroring shader math |
| `engine/shaders/transform_gizmo.slang` | `vsSdfSegmentQuad`, center→disc, arrow stem→segment; fragment paths |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h` | `k_center_verts` 72→6; remove `k_center_sides` |
| `engine/src/tests/gizmo_sdf_aa_test.cpp` | **New** — SDF alpha regression |
| `engine/src/tests/CMakeLists.txt` | Register `gizmo_sdf_aa_test` |
| `engine/src/runtime/function/render/CMakeLists.txt` | Add `gizmo_sdf_aa.cpp` |

**Unchanged:** `transform_gizmo_overlay.cpp` draw calls, `transform_gizmo_pick.cpp`, uniform layout (`TransformGizmoUniformData` stays 288 bytes).

---

### Task 1: CPU SDF helpers + tests (TDD)

**Files:**
- Create: `engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.h`
- Create: `engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.cpp`
- Create: `engine/src/tests/gizmo_sdf_aa_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: `engine/src/runtime/function/render/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `engine/src/tests/gizmo_sdf_aa_test.cpp`:

```cpp
#include <cstdio>
#include <cmath>

#include "runtime/function/render/gizmo/gizmo_sdf_aa.h"

int main() {
  using namespace Blunder;
  int failures = 0;
  auto expect_near = [&](const char* label, const float a, const float b) {
    if (std::fabs(a - b) > 1e-3f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };

  // Disc: interior fully opaque at 1px/px scale
  expect_near("disc interior", discFillAlpha(-2.0f, 1.25f), 1.0f);
  expect_near("disc exterior", discFillAlpha(2.0f, 1.25f), 0.0f);

  // Segment distance: on line center
  expect_near("segment on line", sdSegment2d({0.0f, 5.0f}, {0.0f, 0.0f}, {0.0f, 10.0f}), 0.0f);
  // Perpendicular offset 3
  expect_near("segment perp", sdSegment2d({3.0f, 5.0f}, {0.0f, 0.0f}, {0.0f, 10.0f}), 3.0f);

  // Segment AA: on centerline inside stroke → full alpha
  const float on_stroke = segmentLineAlpha({0.0f, 5.0f}, {0.0f, 0.0f}, {0.0f, 10.0f},
                                           2.0f, 1.0f);
  expect_near("segment stroke center", on_stroke, 1.0f);

  const float far_stroke = segmentLineAlpha({10.0f, 5.0f}, {0.0f, 0.0f}, {0.0f, 10.0f},
                                            2.0f, 1.0f);
  expect_near("segment stroke far", far_stroke, 0.0f);

  if (failures == 0) {
    std::printf("gizmo_sdf_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
```

Add to `engine/src/tests/CMakeLists.txt` (after `transform_gizmo_aa_test` block):

```cmake
add_executable(gizmo_sdf_aa_test gizmo_sdf_aa_test.cpp)
target_link_libraries(gizmo_sdf_aa_test PRIVATE engine_runtime)
if(MSVC)
  target_compile_options(gizmo_sdf_aa_test PRIVATE /Zc:preprocessor)
endif()
add_test(NAME gizmo_sdf_aa_test COMMAND gizmo_sdf_aa_test)
set_tests_properties(gizmo_sdf_aa_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/engine/src/editor/Debug")
```

- [ ] **Step 2: Run test — verify RED**

```powershell
cd e:\Dev\Blunder-Engine\build\vs2026-debug
cmake --build . --target gizmo_sdf_aa_test --config Debug
.\engine\src\tests\Debug\gizmo_sdf_aa_test.exe
```

Expected: FAIL — `gizmo_sdf_aa.h` not found

- [ ] **Step 3: Implement CPU helpers**

`engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.h`:

```cpp
#pragma once

namespace Blunder {

/// Signed distance from point p to segment [a,b] in 2D.
float sdSegment2d(float px, float py, float ax, float ay, float bx, float by);

/// Filled disc AA: signed_px_dist = (r - radius) / units_per_pixel.
float discFillAlpha(float signed_px_dist, float edge_px = 1.25f);

/// Line segment stroke AA in pixel space (mirrors shader segmentLineAlpha).
float segmentLineAlpha(float px, float py, float ax, float ay, float bx, float by,
                       float half_width_px, float units_per_pixel);

}  // namespace Blunder
```

`engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.cpp`:

```cpp
#include "runtime/function/render/gizmo/gizmo_sdf_aa.h"

#include <algorithm>
#include <cmath>

namespace Blunder {

float sdSegment2d(const float px, const float py, const float ax, const float ay,
                  const float bx, const float by) {
  const float pax = px - ax;
  const float pay = py - ay;
  const float bax = bx - ax;
  const float bay = by - ay;
  const float denom = std::max(bax * bax + bay * bay, 1e-6f);
  const float h = std::clamp((pax * bax + pay * bay) / denom, 0.0f, 1.0f);
  const float dx = pax - bax * h;
  const float dy = pay - bay * h;
  return std::sqrt(dx * dx + dy * dy);
}

float discFillAlpha(const float signed_px_dist, const float edge_px) {
  return std::clamp(edge_px - signed_px_dist, 0.0f, 1.0f);
}

float segmentLineAlpha(const float px, const float py, const float ax, const float ay,
                       const float bx, const float by, const float half_width_px,
                       const float units_per_pixel) {
  const float sdf = sdSegment2d(px, py, ax, ay, bx, by);
  const float px_dist = (sdf - half_width_px) / std::max(units_per_pixel, 1e-6f);
  const float a_core = discFillAlpha(-px_dist, 1.25f);
  const float a_fw = discFillAlpha(-px_dist, 1.0f);
  return std::max(a_core, a_fw);
}

}  // namespace Blunder
```

Add to `engine/src/runtime/function/render/CMakeLists.txt` (after `gizmo_polyline_aa.h`):

```cmake
    "function/render/gizmo/gizmo_sdf_aa.cpp"
    "function/render/gizmo/gizmo_sdf_aa.h"
```

- [ ] **Step 4: Run test — verify GREEN**

```powershell
cmake --build build\vs2026-debug --target gizmo_sdf_aa_test --config Debug
.\build\vs2026-debug\engine\src\tests\Debug\gizmo_sdf_aa_test.exe
```

Expected: `gizmo_sdf_aa_test: all passed`

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.h \
        engine/src/runtime/function/render/gizmo/gizmo_sdf_aa.cpp \
        engine/src/tests/gizmo_sdf_aa_test.cpp \
        engine/src/tests/CMakeLists.txt \
        engine/src/runtime/function/render/CMakeLists.txt
git commit -m "test: add CPU SDF AA helpers for gizmo center and arrow stem"
```

---

### Task 2: Center handle → SDF filled disc

**Files:**
- Modify: `engine/shaders/transform_gizmo.slang` (disc tuning, style 2 vertex path)
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h`

- [ ] **Step 1: Update draw counts**

In `engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h`, replace center vert constants:

```cpp
  // Before:
  // static constexpr uint32_t k_center_sides = 12u;
  // static constexpr uint32_t k_center_verts = k_center_sides * 6u;

  // After:
  static constexpr uint32_t k_center_verts = k_sdf_ring_verts;
```

- [ ] **Step 2: Add solid disc alpha + wire center vertex path**

In `engine/shaders/transform_gizmo.slang`, after `discFillAlpha` add:

```slang
float discFillAlphaSolid(float2 local_xy, float radius) {
    float sdf = length(local_xy) - radius;
    float pxDist = sdf / localUnitsPerPixel(local_xy);
    return saturate(1.25 - pxDist);
}
```

In `vertexMain`, replace style `2u` block:

```slang
    if (style == 2u) {
        if (vid >= kCenterVertCount) {
            return emitDiscarded();
        }
        const float r = kMeshCenterRadius * lineWidthScale();
        return vsSdfRingQuad(vid, r, kStrokeSdfDisc, 1.0);
    }
```

Delete `vsCenter()` function entirely (lines ~267–278).

In `fragmentMain`, change disc branch to pick solid vs faint fill:

```slang
    if (input.strokeCoord < -2.5) {
        uint style = uint(ubo.params.x + 0.5);
        if (style == 2u) {
            color.a *= discFillAlphaSolid(input.ringLocalXY, input.ringMajorR);
        } else {
            color.a *= discFillAlpha(input.ringLocalXY, input.ringMajorR);
        }
        if (color.a < 0.001) {
            discard;
        }
        return color;
    }
```

- [ ] **Step 3: Build editor target**

```powershell
cmake --build build\vs2026-debug --target engine_runtime --config Debug
```

Expected: build succeeds (shader recompiles at editor startup)

- [ ] **Step 4: Manual verify — translate center**

1. Launch `engine_editor` from `build\vs2026-debug\engine\src\editor\Debug`
2. Load `assets/Scenes/pick_test.scene.asset`
3. Switch to **Translate** gizmo, select a box
4. Confirm center disc is **smooth circle** (no 12-gon edges), **stays fixed on pivot** when orbiting camera
5. Confirm center still **highlights on hover** and **drags** all axes

- [ ] **Step 5: Commit**

```bash
git add engine/shaders/transform_gizmo.slang \
        engine/src/runtime/function/render/gizmo/transform_gizmo_shared.h
git commit -m "feat(gizmo): render translate center as SDF filled disc"
```

---

### Task 3: Arrow stem → SDF segment line

**Files:**
- Modify: `engine/shaders/transform_gizmo.slang`

- [ ] **Step 1: Add segment stroke marker and helpers**

After `kStrokeSdfDisc` constant block in `transform_gizmo.slang`:

```slang
static const float kStrokeSdfSegment = -4.0;
```

After `ringLineAlpha`, add:

```slang
float sdSegment2d(float2 p, float2 a, float2 b) {
    float2 pa = p - a;
    float2 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h);
}

float segmentLineAlpha(float2 p, float2 a, float2 b) {
    float4 clipC = clipFromLocal(float3(0.0, 0.0, (a.y + b.y) * 0.5));
    float halfWLocal = (ubo.line_width_px + kPolylineSmoothWidth) * 0.5 *
                       (2.0 / max(ubo.viewport_height_px, 1.0)) * abs(clipC.w);
    float sdf = sdSegment2d(p, a, b) - halfWLocal;
    float halfW = max(ubo.line_width_px * 0.5, 0.75);
    float pxDistJacobian = sdf / localUnitsPerPixel(p);
    float aJacobian = saturate(halfW + 1.25 - pxDistJacobian);
    float pxDistFw = sdf / max(fwidth(sdf), localUnitsPerPixel(p) * 1e-3);
    float aFw = saturate(halfW + 1.25 - pxDistFw);
    return max(aJacobian, aFw);
}
```

Add vertex helper after `vsSdfRingQuad`:

```slang
// Local XZ plane segment (ringLocalXY = (x, z)).
VertexOutput vsSdfSegmentQuad(uint vid, float2 seg_a_xz, float2 seg_b_xz, float pad_scale) {
    float4 clipC = clipFromLocal(float3(0.0, 0.0, (seg_a_xz.y + seg_b_xz.y) * 0.5));
    float halfWLocal = (ubo.line_width_px + kPolylineSmoothWidth) * 0.5 *
                       (2.0 / max(ubo.viewport_height_px, 1.0)) * abs(clipC.w);
    float pxPad = ubo.line_width_px + kPolylineSmoothWidth + 2.0;
    float padLocal = pxPad * 0.5 * (2.0 / max(ubo.viewport_height_px, 1.0)) * abs(clipC.w) * pad_scale;

    float2 extMin = min(seg_a_xz, seg_b_xz) - float2(halfWLocal + padLocal, halfWLocal + padLocal);
    float2 extMax = max(seg_a_xz, seg_b_xz) + float2(halfWLocal + padLocal, halfWLocal + padLocal);

    float2 uv = quadUV(vid);
    float2 p_xz = lerp(extMin, extMax, uv);

    VertexOutput o;
    o.sv_position = projectLocal(float3(p_xz.x, 0.0, p_xz.y));
    o.worldNormal = normalFromLocal(float3(0, 1, 0));
    o.color = float4(ubo.color.rgb, ubo.color.a * ubo.params.y);
    o.worldPos = worldFromLocal(float3(p_xz.x, 0.0, p_xz.y));
    o.strokeCoord = kStrokeSdfSegment;
    o.ringLocalXY = p_xz;
    o.ringMajorR = length(seg_b_xz - seg_a_xz);
    return o;
}
```

- [ ] **Step 2: Wire arrow stem to segment quad**

In `vsArrow`, replace stem branch (`vid < kArrowStemVertCount`):

```slang
    if (vid < kArrowStemVertCount) {
        return vsSdfSegmentQuad(vid, float2(0.0, 0.0), float2(0.0, stemLen), 1.0);
    }
```

- [ ] **Step 3: Add fragment path for segment**

In `fragmentMain`, insert **before** disc branch (`strokeCoord < -2.5`):

```slang
    if (input.strokeCoord < -3.5) {
        color.a *= segmentLineAlpha(input.ringLocalXY, float2(0.0, 0.0),
                                    float2(0.0, kMeshStemLength));
        if (color.a < 1e-4) {
            discard;
        }
        return color;
    }
```

Update SDF clip rebuild condition:

```slang
    const bool isSdfPath = input.strokeCoord < -1.5;
```

→ change to:

```slang
    const bool isSdfPath = input.strokeCoord < -1.5; // rings (-2), discs (-3), segments (-4)
```

(No code change needed — `-4 < -1.5` already true.)

- [ ] **Step 4: Build + run unit tests**

```powershell
cmake --build build\vs2026-debug --target gizmo_sdf_aa_test transform_gizmo_aa_test --config Debug
.\build\vs2026-debug\engine\src\tests\Debug\gizmo_sdf_aa_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\transform_gizmo_aa_test.exe
```

Expected: both print `all passed`

- [ ] **Step 5: Manual verify — translate arrows**

1. Launch editor, translate gizmo on pick_test scene
2. Orbit camera — arrow **stems stay attached** to axis origin, smooth edges (no stair-steps)
3. Arrow **cone tips** unchanged (solid mesh)
4. Hover + drag X/Y/Z arrows still works (`transform_gizmo_hover_test` optional):

```powershell
.\build\vs2026-debug\engine\src\tests\Debug\transform_gizmo_hover_test.exe
```

Expected: `transform_gizmo_hover_test: all passed`

- [ ] **Step 6: Commit**

```bash
git add engine/shaders/transform_gizmo.slang
git commit -m "feat(gizmo): render arrow stem with local segment SDF AA"
```

---

### Task 4: Validation + plan close-out

**Files:**
- Modify: `openspec/changes/outline-gizmo-anti-aliasing/tasks.md` (optional checkbox notes)

- [ ] **Step 1: Run full gizmo test suite**

```powershell
cmake --build build\vs2026-debug --target outline_aa_test gizmo_polyline_blender_aa_test transform_gizmo_aa_test gizmo_sdf_aa_test gizmo_dial_sides_test transform_gizmo_hover_test --config Debug
$tests = @(
  "outline_aa_test","gizmo_polyline_blender_aa_test","transform_gizmo_aa_test",
  "gizmo_sdf_aa_test","gizmo_dial_sides_test","transform_gizmo_hover_test"
)
foreach ($t in $tests) {
  & "build\vs2026-debug\engine\src\tests\Debug\$t.exe"
}
```

Expected: each prints `all passed`

- [ ] **Step 2: Visual regression checklist**

| Handle | Mode | Pass criteria |
|--------|------|---------------|
| Center disc | Translate | Smooth circle, pivot-locked, hover OK |
| Arrow stem | Translate | Smooth line, axis-locked, cone OK |
| RGB rings | Rotate | Unchanged (no regression) |
| White outer ring | Rotate | Unchanged, no camera drift |

- [ ] **Step 3: Mark OpenSpec tasks (optional)**

In `openspec/changes/outline-gizmo-anti-aliasing/tasks.md`, add under completed notes:

```markdown
- [x] Batch 1: translate center SDF disc + arrow stem SDF segment (2026-07-06)
```

- [ ] **Step 4: Final commit (if OpenSpec touched)**

```bash
git add openspec/changes/outline-gizmo-anti-aliasing/tasks.md
git commit -m "docs: mark gizmo SDF batch 1 complete in openspec tasks"
```

---

## Self-review

| Requirement | Task |
|-------------|------|
| Center → `discFillAlpha` family | Task 2 |
| Arrow stem → segment SDF | Task 3 |
| CPU TDD mirrors | Task 1 |
| Same 3D anchor as rotate rings | Tasks 2–3 (local `vsSdf*Quad`, no screen-pixel path) |
| Pick/hover unchanged | Explicitly out of scope — analytic pick |
| No uniform layout change | Confirmed — reuses `ringLocalXY` / `ringMajorR` |

**Placeholder scan:** None — all steps include concrete code and commands.

---

## Execution handoff

**Plan complete and saved to `docs/superpowers/plans/2026-07-06-gizmo-sdf-batch1-center-arrow.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** — Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
