# Gizmo Hover Style — Blender Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align transform gizmo **hover visual style** with Blender 4.x (`gizmo_get_axis_color` + `gizmo_color_get`), without changing hover interaction (pick, input, redraw) already landed in `gizmo-handle-hover`.

**Architecture:** Blender stores per-axis `color` / `color_hi` each frame; draw paths call `gizmo_color_get(gz, is_highlight, out)`. Blunder mirrors this with `gizmoAxisColor(axis, idot)` → pair, and `gizmoColorGet(axis, idot, highlight)` at draw time. Hover state (`isHandleHighlighted`) is already wired — this plan only closes style gaps and adds regression tests against Blender reference constants.

**Tech Stack:** C++20, `transform_gizmo_types.cpp`, `transform_gizmo_overlay.cpp`, `transform_gizmo_hover_test.cpp`, Blender reference at `e:\Dev\blender\source\blender\editors\transform\transform_gizmo_3d.cc` + `gizmo_library_utils.cc`.

**Prerequisite:** `gizmo-handle-hover` tasks §1–4 complete (hover pick + `gizmoColorGet` skeleton + alpha 0.6/1.0).

**Out of scope:** `WM_GIZMO_DRAW_HOVER` show-on-hover visibility for `rot_t` trackball; cursor changes; theme editor / `TH_AXIS_*` runtime loading; line-width change on hover (Blender does not change line width on highlight).

---

## Blender reference map

| Blender | Blunder equivalent | Style rule |
|---------|-------------------|------------|
| `gizmo_get_axis_color()` L325–395 `transform_gizmo_3d.cc` | `gizmoAxisColor()` | RGB from theme; `color.a = 0.6 * alpha_fac`; `color_hi.a = 1.0 * alpha_fac` |
| `gizmo_color_get()` L160–168 `gizmo_library_utils.cc` | `gizmoColorGet()` | `highlight ? color_hi : color` (no `WM_GIZMO_DRAW_HOVER` on axis handles) |
| `g_tw_axis_range` L99–105 | `k_axis_idot_fade_*` / `k_plane_idot_fade_*` | Axis fade 0.02–0.1; plane fade 0.175–0.25 |
| `MAN_AXIS_ROT_T` alpha_fac 0.05 | `k_trackball_alpha_factor` | Trackball alpha = 0.6 × 0.05 = **0.03** |
| Rotation dials `alpha_fac = 1.0` | `gizmoAxisFadeFactor` rot branch | Hover always full 1.0 alpha on rings |
| `arrow3d` / `dial3d` draw | `drawTranslateHandle`, `drawRotationDial`, … | Pass `highlight` from `WM_GIZMO_STATE_HIGHLIGHT` ≡ `isHandleHighlighted` |

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/gizmo/transform_gizmo_types.h` | Alpha constants, optional `gizmoHandleUsesHoverColorHi(axis)` |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_types.cpp` | `gizmoAxisColor`, `gizmoColorGet` — single style source |
| `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp` | All draws use `gizmoColorGet` + `color.a` (no floors, no hardcoded alpha_hi) |
| `engine/src/tests/transform_gizmo_hover_test.cpp` | Blender parity color tests |
| `openspec/changes/gizmo-handle-hover/specs/transform-gizmo/spec.md` | Clarify alpha 0.6 / 1.0 in hover requirement |
| `docs/agents/render-pipeline.md` | Already documents hover color path — verify after tasks |

---

## Task 1: Style audit — overlay uses `gizmoColorGet` everywhere

**Files:**
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp`
- Test: `engine/src/tests/transform_gizmo_hover_test.cpp` (no new test yet)

- [ ] **Step 1: Grep audit**

Run:

```powershell
rg "gizmo_color_alpha_hi|centerColor|axisColorFor|std::max\(color\.a|gizmoAxisColor\(" engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp
```

Expected after task: **no matches** except `gizmoColorGet` for interactive handle colors.

- [ ] **Step 2: Fix any stragglers**

Interactive handles (translate / scale / rotate axis dials) must follow this pattern:

```cpp
const glm::vec4 color = gizmoColorGet(axis, idot, highlight);
recordDraw(cmd, state, handle_matrix, glm::vec4(glm::vec3(color), 1.0f), style,
           color.a, /* ... */);
```

Decor-only draws (`rot_t`, `rot_c`, `scale_c_outer`) use `gizmoColorGet(..., false)` — never hover highlight (not pickable).

Ghost dial (`drawRotationDial` with `ghost=true`) keeps hardcoded gray `0.5` — matches Blender modal ghost.

- [ ] **Step 3: Build**

```powershell
cmake --build build/vs2026-debug --target engine_runtime --config Debug
```

Expected: success.

- [ ] **Step 4: Commit**

```bash
git add engine/src/runtime/function/render/gizmo/transform_gizmo_overlay.cpp
git commit -m "refactor: route all gizmo handle draws through gizmoColorGet"
```

---

## Task 2: Blender parity unit tests (TDD for edge alphas)

**Files:**
- Modify: `engine/src/tests/transform_gizmo_hover_test.cpp`

- [ ] **Step 1: Add failing tests for Blender `gizmo_get_axis_color` cases**

Append to `main()` in `transform_gizmo_hover_test.cpp`:

```cpp
  {
    const float idot[3] = {1.0f, 1.0f, 1.0f};
    const GizmoAxisColor rot_t = gizmoAxisColor(ManipulatorAxis::rot_t, idot);
    expect_true("rot_t normal alpha 0.03",
                std::fabs(rot_t.color.a - 0.03f) < 1e-4f);
    expect_true("rot_t hover alpha 0.05",
                std::fabs(gizmoColorGet(ManipulatorAxis::rot_t, idot, true).a - 0.05f) < 1e-4f);
  }
  {
    const float idot_edge[3] = {0.06f, 1.0f, 1.0f};
    const GizmoAxisColor x_faded = gizmoAxisColor(ManipulatorAxis::trans_x, idot_edge);
    expect_true("faded axis normal alpha < 0.6",
                x_faded.color.a > 0.0f && x_faded.color.a < 0.6f);
    expect_true("faded axis hover alpha > normal",
                gizmoColorGet(ManipulatorAxis::trans_x, idot_edge, true).a >
                    x_faded.color.a);
  }
  {
    const float idot[3] = {0.0f, 0.0f, 1.0f};
    const GizmoAxisColor rot_x = gizmoAxisColor(ManipulatorAxis::rot_x, idot);
    expect_true("rotation dial ignores idot fade",
                std::fabs(rot_x.color.a - 0.6f) < 1e-4f);
    expect_true("rotation dial hover full alpha",
                std::fabs(gizmoColorGet(ManipulatorAxis::rot_x, idot, true).a - 1.0f) < 1e-4f);
  }
```

- [ ] **Step 2: Run test — verify RED or GREEN**

```powershell
cmake --build build/vs2026-debug --target transform_gizmo_hover_test --config Debug
Set-Location build\vs2026-debug\engine\src\editor\Debug
& "e:\Dev\Blunder-Engine\build\vs2026-debug\engine\src\tests\Debug\transform_gizmo_hover_test.exe"
```

If **rot_t hover** test fails (expected `0.05` vs actual `0.03`): fix `gizmoAxisColor` so `rot_t` uses `alpha_fac = 0.05` for **both** normal and hi (Blender: `alpha_hi * 0.05 = 0.05`, not `1.0 * 0.05` only on hi). Blender sets `alpha_fac = 0.05` then `r_col[3] = 0.6*0.05`, `r_col_hi[3] = 1.0*0.05` → hi is **0.05**, not 0.03.

- [ ] **Step 3: Fix `gizmoAxisColor` if rot_t hi test failed**

In `transform_gizmo_types.cpp`, ensure `rot_t` branch sets `alpha_fac = k_trackball_alpha_factor` **before** applying both alphas (already should). Re-run test until all pass.

- [ ] **Step 4: Commit**

```bash
git add engine/src/tests/transform_gizmo_hover_test.cpp
git commit -m "test: blender parity cases for gizmo hover alpha"
```

---

## Task 3: `gizmoColorGet` documentation + `WM_GIZMO_DRAW_HOVER` guard

**Files:**
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_types.h`
- Modify: `engine/src/runtime/function/render/gizmo/transform_gizmo_types.cpp`

- [ ] **Step 1: Add axis hover-color policy helper**

In `transform_gizmo_types.h`:

```cpp
/// True when hover should select color_hi (Blender: !(gz->flag & WM_GIZMO_DRAW_HOVER)).
/// Decor axes (rot_t) are not hover-picked; always false for them today.
bool gizmoHandleUsesHoverColorHi(ManipulatorAxis axis);
```

In `transform_gizmo_types.cpp`:

```cpp
bool gizmoHandleUsesHoverColorHi(const ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::rot_t:
    case ManipulatorAxis::rot_c:
    case ManipulatorAxis::scale_c_outer:
      return false;
    default:
      return true;
  }
}
```

- [ ] **Step 2: Update `gizmoColorGet`**

```cpp
glm::vec4 gizmoColorGet(const ManipulatorAxis axis, const float idot[3],
                        const bool highlight) {
  const GizmoAxisColor colors = gizmoAxisColor(axis, idot);
  const bool use_hi = highlight && gizmoHandleUsesHoverColorHi(axis);
  return use_hi ? colors.color_hi : colors.color;
}
```

- [ ] **Step 3: Add test**

```cpp
  expect_true("rot_t ignores hover color_hi",
              std::fabs(gizmoColorGet(ManipulatorAxis::rot_t, idot, true).a -
                        gizmoAxisColor(ManipulatorAxis::rot_t, idot).color.a) < 1e-6f);
```

Run `transform_gizmo_hover_test.exe` — Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add engine/src/runtime/function/render/gizmo/transform_gizmo_types.h \
        engine/src/runtime/function/render/gizmo/transform_gizmo_types.cpp \
        engine/src/tests/transform_gizmo_hover_test.cpp
git commit -m "feat: gizmoColorGet respects non-hoverable decor axes"
```

---

## Task 4: OpenSpec + docs style wording

**Files:**
- Modify: `openspec/changes/gizmo-handle-hover/specs/transform-gizmo/spec.md`
- Modify: `docs/agents/render-pipeline.md`

- [ ] **Step 1: Extend delta spec scenario**

Under `### Requirement: Gizmo handle hover highlight`, add:

```markdown
#### Scenario: Hover uses Blender alpha parity

- **WHEN** an interactive axis handle is hovered
- **THEN** its color RGB matches the normal state
- **AND** its alpha is `k_gizmo_color_alpha_hi` (1.0) multiplied by the same `alpha_fac` as `gizmo_get_axis_color`
- **AND** non-interactive decor (trackball, outer ring, scale annulus) do not switch to `color_hi` on hover
```

- [ ] **Step 2: Verify render-pipeline.md** mentions 0.6 / 1.0 (already added — adjust if Task 2 changes rot_t hi to 0.05).

- [ ] **Step 3: Validate**

```powershell
openspec validate gizmo-handle-hover
```

- [ ] **Step 4: Commit**

```bash
git add openspec/changes/gizmo-handle-hover/specs/transform-gizmo/spec.md docs/agents/render-pipeline.md
git commit -m "docs: clarify blender hover alpha parity in spec"
```

---

## Task 5: Manual visual QA (style only)

**Files:** none

- [ ] **Step 1: Side-by-side with Blender**

Open Blender + Blunder editor, same default theme, select cube, `G` mode:

| Check | Blender | Blunder |
|-------|---------|---------|
| Idle X arrow alpha | ~0.6 | ~0.6 |
| Hover X arrow alpha | 1.0 | 1.0 |
| Hover RGB unchanged | same red | same red |
| Rotate ring hover | brighter, same hue | same |
| Plane handle hover | visible hi | same |
| Trackball | no hi on hover | no hi (decor) |

- [ ] **Step 2: Mark OpenSpec manual tasks**

In `openspec/changes/gizmo-handle-hover/tasks.md`:

```markdown
- [x] 5.3 Manual: G/R/S — hover each interactive handle type; color_hi visible
- [x] 5.4 Manual: drag handle — hover suppressed, active stays highlighted
```

(5.5 is interaction — already verified separately.)

- [ ] **Step 3: Commit tasks.md if checked**

```bash
git add openspec/changes/gizmo-handle-hover/tasks.md
git commit -m "chore: mark gizmo hover style manual QA complete"
```

---

## Self-review

| Blender style rule | Task |
|--------------------|------|
| `color` alpha 0.6 × alpha_fac | Task 2 tests + existing `gizmoAxisColor` |
| `color_hi` alpha 1.0 × alpha_fac | Task 2 tests |
| `gizmo_color_get` highlight path | Task 1 + Task 3 |
| rot_t faint overlay | Task 2 rot_t tests |
| rot rings no idot fade | Task 2 rot_x test |
| `WM_GIZMO_DRAW_HOVER` decor exclusion | Task 3 |
| No line-width hover change | Out of scope (documented) |

**Placeholder scan:** None.

**Type consistency:** `gizmoColorGet(axis, idot, highlight)` used throughout; `gizmoHandleUsesHoverColorHi` only gates hi selection.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-04-gizmo-hover-style-blender-parity.md`. Two execution options:

**1. Subagent-Driven (recommended)** — fresh subagent per task, spec + code review between tasks

**2. Inline Execution** — run tasks in this session with checkpoints

Which approach?
