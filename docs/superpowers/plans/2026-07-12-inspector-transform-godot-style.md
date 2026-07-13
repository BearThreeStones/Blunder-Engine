# Godot-style Inspector Transform Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the stacked Inspector Transform sliders with a Godot-like Local Transform editor: horizontal axis number fields (type + scrub), rotation under-sliders, Scale link, Euler/Quaternion Rotation Edit Mode, and multi-select Absolute/Delta apply.

**Architecture:** Keep Entity TRS storage unchanged (local Position + Quaternion + Scale). Put pure edit math and multi-select apply rules in `inspector_transform_ops` (unit-tested). Slint owns presentation (`AxisNumberField` + Transform section). `SlintSystem::syncInspectorFromSelection` / `applyInspectorTransform` become thin adapters that read selection + session UI state and call the ops. Euler view reuses `SceneSerializer` quaternion↔euler helpers (fixed engine order). No Undo.

**Tech Stack:** C++20, GLM quats, Slint UI, existing `UiHost` `inspectorTransformEdited` event, `EditorSelectionSystem`, CTest-style `main()` tests linked to `engine_runtime`.

**Decisions (grilled):** See `CONTEXT.md` § Inspector Transform and `docs/adr/0001-inspector-authoritative-quaternion.md`, `docs/adr/0002-inspector-multi-edit-absolute-delta.md`.

**OpenSpec (before code):** `/opsx:propose inspector-transform-godot-style`

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/editor/inspector_transform_ops.h` | Pure ops: scale-link factor, normalize quat, mixed detection, Absolute/Delta apply to one component, Euler delta on quat |
| `engine/src/runtime/function/editor/inspector_transform_ops.cpp` | Implementations (Euler via `SceneSerializer`) |
| `engine/src/tests/inspector_transform_ops_test.cpp` | TDD for ops |
| `engine/src/tests/CMakeLists.txt` | Register test |
| `engine/src/runtime/CMakeLists.txt` | Compile new ops sources |
| `engine/src/runtime/function/slint/axis_number_field.slint` | Reusable axis field: color letter, text, unit, scrub |
| `engine/src/runtime/function/slint/inspector_panel.slint` | Godot-like Transform section + session toggles |
| `engine/src/runtime/function/slint/editor_window.slint` | New inspector properties / callbacks plumbing |
| `engine/src/runtime/function/slint/slint_system.h` | Session state + focused-field lock members |
| `engine/src/runtime/function/slint/slint_system.cpp` | Multi-select sync/apply, mode wiring, focus lock |
| `engine/src/runtime/function/ui/docking/dock_floating_window_host.h` | Snapshot fields for new inspector props (if native float Inspector is used) |
| `engine/src/runtime/function/ui/docking/dock_floating_window_host.cpp` | Push/pull new snapshot fields |
| `openspec/changes/inspector-transform-godot-style/` | Proposal / design / tasks / spec |
| `docs/adr/0001-*.md`, `0002-*.md` | Already written in grilling |
| `CONTEXT.md` | Already updated in grilling |

**Out of scope:** Rotation Order dropdown; Basis mode; World/Global Transform editor; Undo; restyling shading section; changing `SceneSerializer` file euler convention.

---

### Task 1: OpenSpec change skeleton

**Files:**
- Create via CLI: `openspec/changes/inspector-transform-godot-style/`

- [ ] **Step 1: Propose the change**

```powershell
openspec new change "inspector-transform-godot-style"
```

Fill proposal/design/tasks (or `/opsx:propose inspector-transform-godot-style`) with:

- **Problem:** Inspector Transform is vertical sliders only; lacks typed fields, scrub, Scale link, Euler/Quaternion mode, and multi-select Absolute/Delta.
- **Solution:** Godot-like Local Transform UI + `inspector_transform_ops` + SlintSystem multi-select apply.
- **Spec capability** `inspector-transform` (new):

```markdown
### Requirement: Local Transform Inspector

The Inspector SHALL edit the selected entities' local Position, Rotation, and Scale with horizontal axis number fields (typed commit + live scrub), rotation under-sliders in Euler mode, Scale link, and Rotation Edit Mode Euler|Quaternion.

#### Scenario: Single-select position type-commit

- **WHEN** one entity is selected and the user types a Position X value and presses Enter
- **THEN** that entity's local position.x SHALL equal the parsed value
- **AND** the viewport SHALL update without requiring camera motion

#### Scenario: Scale link proportional edit

- **WHEN** Scale link is on and Scale is (2, 1, 0.5)
- **AND** the user sets Scale X to 4
- **THEN** Scale SHALL become (4, 2, 1)

#### Scenario: Multi-select Absolute position

- **WHEN** multiple entities are selected with Multi-edit mode Absolute
- **AND** the user commits Position X = 5
- **THEN** every selected entity's local position.x SHALL be 5

#### Scenario: Multi-select Delta rotation

- **WHEN** multiple entities are selected
- **AND** the user applies a Rotation X delta of 10 degrees
- **THEN** each selected entity's authoritative Quaternion SHALL be updated by the same Euler X delta through the fixed engine Euler convention
```

- [ ] **Step 2: Commit OpenSpec artifacts**

```powershell
git add openspec/changes/inspector-transform-godot-style docs/adr CONTEXT.md
git commit -m "$(cat <<'EOF'
docs: propose inspector-transform-godot-style and record ADRs

EOF
)"
```

---

### Task 2: TDD `inspector_transform_ops` (RED → GREEN)

**Files:**
- Create: `engine/src/runtime/function/editor/inspector_transform_ops.h`
- Create: `engine/src/runtime/function/editor/inspector_transform_ops.cpp`
- Create: `engine/src/tests/inspector_transform_ops_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: `engine/src/runtime/CMakeLists.txt` (add the two new editor files next to other `function/editor/` entries — search for `editor_selection_system.cpp` and add siblings)

- [ ] **Step 1: Write the failing test**

Create `engine/src/tests/inspector_transform_ops_test.cpp`:

```cpp
#include "runtime/function/editor/inspector_transform_ops.h"
#include "runtime/function/scene/scene_serializer.h"

#include <cassert>
#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

bool near3(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4f) {
  return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const glm::vec3 linked =
        applyScaleLinkEdit(glm::vec3(2.0f, 1.0f, 0.5f), 0, 4.0f);
    expect_true("scale link doubles all axes",
                near3(linked, glm::vec3(4.0f, 2.0f, 1.0f)));
  }

  {
    const glm::vec3 linked =
        applyScaleLinkEdit(glm::vec3(0.0f, 1.0f, 1.0f), 0, 2.0f);
    expect_true("near-zero edited axis does not propagate",
                near3(linked, glm::vec3(2.0f, 1.0f, 1.0f)));
  }

  {
    expect_true("components equal => not mixed",
                !isMixedComponent(1.0f, 1.0f));
    expect_true("components differ => mixed", isMixedComponent(1.0f, 2.0f));
  }

  {
    float a = 1.0f;
    float b = 10.0f;
    applyAbsoluteComponent(5.0f, a);
    applyAbsoluteComponent(5.0f, b);
    expect_true("absolute sets both", near(a, 5.0f) && near(b, 5.0f));
  }

  {
    float a = 1.0f;
    float b = 10.0f;
    applyDeltaComponent(2.0f, a);
    applyDeltaComponent(2.0f, b);
    expect_true("delta adds both", near(a, 3.0f) && near(b, 12.0f));
  }

  {
    const Quat q0 = glm::identity<Quat>();
    const Quat q1 = applyEulerDeltaDegrees(q0, glm::vec3(10.0f, 0.0f, 0.0f));
    const Vec3 e = SceneSerializer::rotationToEulerDegrees(q1);
    expect_true("euler delta ~10 on X", near(e.x, 10.0f, 0.05f));
  }

  {
    Quat q(2.0f, 0.0f, 0.0f, 0.0f);
    q = normalizeQuaternion(q);
    expect_true("normalize unit length",
                near(glm::length(q), 1.0f));
  }

  {
    const Quat q = SceneSerializer::rotationFromEulerDegrees(
        Vec3(15.0f, -20.0f, 35.0f));
    const Vec3 e = SceneSerializer::rotationToEulerDegrees(q);
    const Quat q2 = SceneSerializer::rotationFromEulerDegrees(e);
    expect_true("euler roundtrip stable",
                near(glm::dot(q, q2), 1.0f, 1e-3f) ||
                    near(glm::dot(q, q2), -1.0f, 1e-3f));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "inspector_transform_ops_test: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("inspector_transform_ops_test: all passed\n");
  return 0;
}
```

- [ ] **Step 2: Register test + declare API so the build fails (missing symbols)**

Append to `engine/src/tests/CMakeLists.txt`:

```cmake
add_executable(inspector_transform_ops_test
    "inspector_transform_ops_test.cpp"
)

target_link_libraries(inspector_transform_ops_test
    PRIVATE engine_runtime
)

if(MSVC)
    target_compile_options(inspector_transform_ops_test PRIVATE /Zc:preprocessor)
endif()

target_precompile_headers(inspector_transform_ops_test
    REUSE_FROM engine_runtime
)

add_test(NAME inspector_transform_ops_test
         COMMAND inspector_transform_ops_test)
```

Create `engine/src/runtime/function/editor/inspector_transform_ops.h`:

```cpp
#pragma once

#include "runtime/core/math/math_types.h"

namespace Blunder {

/// Proportional scale-link edit. `axis` is 0=X,1=Y,2=Z. If |old[axis]| < eps,
/// only that axis is set to `new_value`.
Vec3 applyScaleLinkEdit(const Vec3& old_scale, int axis, float new_value,
                        float eps = 1e-6f);

bool isMixedComponent(float a, float b, float eps = 1e-5f);

void applyAbsoluteComponent(float value, float& component);
void applyDeltaComponent(float delta, float& component);

/// Apply Euler-degree delta through SceneSerializer compose/decompose.
Quat applyEulerDeltaDegrees(const Quat& current, const Vec3& delta_degrees);

Quat normalizeQuaternion(const Quat& q);

}  // namespace Blunder
```

Add the `.h`/`.cpp` paths to `engine/src/runtime/CMakeLists.txt` beside `editor_selection_system` sources.

- [ ] **Step 3: Run test — expect link/compile failure or FAIL**

```powershell
cmake --build build/vs2026-debug --config Debug --target inspector_transform_ops_test
```

Expected: fails (missing `.cpp` implementations) or test binary missing.

- [ ] **Step 4: Implement ops**

Create `engine/src/runtime/function/editor/inspector_transform_ops.cpp`:

```cpp
#include "runtime/function/editor/inspector_transform_ops.h"

#include "runtime/function/scene/scene_serializer.h"

#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace Blunder {

Vec3 applyScaleLinkEdit(const Vec3& old_scale, int axis, float new_value,
                        float eps) {
  Vec3 out = old_scale;
  if (axis < 0 || axis > 2) {
    return out;
  }
  const float old_axis = old_scale[axis];
  if (std::fabs(old_axis) < eps) {
    out[axis] = new_value;
    return out;
  }
  const float factor = new_value / old_axis;
  out = old_scale * factor;
  return out;
}

bool isMixedComponent(float a, float b, float eps) {
  return std::fabs(a - b) > eps;
}

void applyAbsoluteComponent(float value, float& component) {
  component = value;
}

void applyDeltaComponent(float delta, float& component) {
  component += delta;
}

Quat applyEulerDeltaDegrees(const Quat& current, const Vec3& delta_degrees) {
  const Vec3 euler = SceneSerializer::rotationToEulerDegrees(current);
  return SceneSerializer::rotationFromEulerDegrees(euler + delta_degrees);
}

Quat normalizeQuaternion(const Quat& q) {
  const float len = glm::length(q);
  if (len < 1e-8f) {
    return glm::identity<Quat>();
  }
  return q / len;
}

}  // namespace Blunder
```

- [ ] **Step 5: Run test — expect PASS**

```powershell
cmake --build build/vs2026-debug --config Debug --target inspector_transform_ops_test
ctest --test-dir build/vs2026-debug -C Debug -R inspector_transform_ops_test --output-on-failure
```

Expected: `inspector_transform_ops_test: all passed`

- [ ] **Step 6: Commit**

```powershell
git add engine/src/runtime/function/editor/inspector_transform_ops.h `
  engine/src/runtime/function/editor/inspector_transform_ops.cpp `
  engine/src/tests/inspector_transform_ops_test.cpp `
  engine/src/tests/CMakeLists.txt `
  engine/src/runtime/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: add inspector transform ops with tests

EOF
)"
```

---

### Task 3: Slint `AxisNumberField` + Transform section UI

**Files:**
- Create: `engine/src/runtime/function/slint/axis_number_field.slint`
- Modify: `engine/src/runtime/function/slint/inspector_panel.slint`
- Modify: `engine/src/runtime/function/slint/editor_window.slint`

- [ ] **Step 1: Add `axis_number_field.slint`**

```slint
import { LineEdit, HorizontalBox, VerticalBox, Slider } from "std-widgets.slint";

/// One Transform axis cell: colored letter, editable value, unit, optional scrub.
export component AxisNumberField inherits Rectangle {
    in property <string> axis-label: "x";
    in property <color> axis-color: #ff3352;
    in-out property <string> text: "0.0";
    in property <string> unit: "";
    in property <bool> show-slider: false;
    in-out property <float> slider-value: 0.0;
    in property <float> slider-min: -180.0;
    in property <float> slider-max: 180.0;
    in property <bool> mixed: false;
    callback scrubbed(float);
    callback committed(string);
    callback editing-started();
    callback editing-cancelled();
    callback slider-edited();

    private property <bool> scrubbing: false;
    private property <float> scrub-origin-x: 0.0;

    background: #252525;
    border-radius: 3px;
    min-height: 22px;
    horizontal-stretch: 1;

    VerticalBox {
        spacing: 2px;
        padding: 0px;

        HorizontalBox {
            spacing: 4px;
            padding-left: 4px;
            padding-right: 4px;
            padding-top: 2px;
            padding-bottom: 2px;

            Text {
                text: root.axis-label;
                color: root.axis-color;
                font-size: 11px;
                font-weight: 700;
                vertical-alignment: center;
                width: 10px;
            }

            TouchArea {
                horizontal-stretch: 1;
                mouse-cursor: ew-resize;
                moved => {
                    if (self.pressed) {
                        if (!root.scrubbing) {
                            root.scrubbing = true;
                            root.scrub-origin-x = self.mouse-x / 1px;
                            root.editing-started();
                        }
                        root.scrubbed((self.mouse-x / 1px) - root.scrub-origin-x);
                    }
                }
                pointer-event(event) => {
                    if (event.kind == PointerEventKind.up && root.scrubbing) {
                        root.scrubbing = false;
                    }
                }

                HorizontalBox {
                    spacing: 2px;
                    LineEdit {
                        horizontal-stretch: 1;
                        text: root.mixed ? "—" : root.text;
                        edited => { root.text = self.text; }
                        accepted => { root.committed(self.text); }
                    }
                    Text {
                        text: root.unit;
                        color: #808080;
                        font-size: 10px;
                        vertical-alignment: center;
                    }
                }
            }
        }

        if root.show-slider: Slider {
            height: 14px;
            minimum: root.slider-min;
            maximum: root.slider-max;
            value <=> root.slider-value;
            changed => { root.slider-edited(); }
        }
    }
}
```

Note: Slint `LineEdit` focus APIs vary by fork version — if Enter/focus-out commit is incomplete, add `field-focus-changed(int field-id, bool focused)` from a `FocusScope` wrapper. Adjust against `docs/agents/slint-fork.md` at compile time. Do not invent a second commit model.

- [ ] **Step 2: Replace Transform block in `inspector_panel.slint`**

Remove the nine stacked `TransformSlider` rows for Position/Rotation/Scale. Import `AxisNumberField`. Add:

- Collapsible `transform-expanded` bool (default true)
- `rotation-edit-mode-euler` bool (true = Euler)
- `scale-link-enabled` bool (default true)
- `multi-edit-visible` bool
- `multi-edit-absolute` bool (true = Absolute)
- Mixed flags: `pos-x-mixed` … through scale (nine bools)
- Quaternion fields: `quat-x/y/z/w` as `in-out float`
- Keep `transform-edited()` for live scrub/slider; keyboard `accepted` also fires it (C++ reads properties)
- Callbacks: `rotation-mode-changed()`, `scale-link-toggled()`, `multi-edit-mode-changed()`
- Callback: `field-focus-changed(int field-id, bool focused)` with ids PositionX=0 … ScaleZ=8, QuatX=9…

Layout (Blunder colors, Godot structure):

```
[v] Transform
  entity name / "N selected"
  [Multi-edit: Absolute | Delta]   // only if multi-edit-visible
  Position
    [x][field m] [y][field m] [z][field m]
  Rotation
    Euler: three AxisNumberField with show-slider
    Quaternion: four fields (no sliders) — hide when multi-edit-visible
  Scale
    [x][field] [y][field] [z][field] [link toggle]
  Rotation Edit Mode  [Euler / Quaternion]
```

Axis colors (match `kAxisColorPositive*`): X `#ff3352`, Y `#8bdc00`, Z `#2890ff`.

Keep Blinn-Phong / SSAO blocks unchanged below a separator.

- [ ] **Step 3: Plumb new properties on `MainEditorWindow`**

Mirror every new Inspector `in`/`in-out`/callback on the root window and bind them into both docked and auto-hide `InspectorPanel` instances (existing `transform-edited =>` sites ~lines 356 and 590 in `editor_window.slint`).

Update `floating_panel_window.slint` if it mirrors `inspector-pos-*` bindings the same way.

- [ ] **Step 4: Build Slint / editor to catch compile errors**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
```

Expected: compiles (C++ may still ignore new props until Task 4).

- [ ] **Step 5: Commit**

```powershell
git add engine/src/runtime/function/slint/axis_number_field.slint `
  engine/src/runtime/function/slint/inspector_panel.slint `
  engine/src/runtime/function/slint/editor_window.slint `
  engine/src/runtime/function/slint/floating_panel_window.slint
git commit -m "$(cat <<'EOF'
feat: add Godot-like Inspector Transform Slint UI

EOF
)"
```

---

### Task 4: Wire `SlintSystem` sync/apply + session state + focus lock

**Files:**
- Modify: `engine/src/runtime/function/slint/slint_system.h`
- Modify: `engine/src/runtime/function/slint/slint_system.cpp`
- Modify: `engine/src/runtime/function/ui/docking/dock_floating_window_host.h`
- Modify: `engine/src/runtime/function/ui/docking/dock_floating_window_host.cpp`

- [ ] **Step 1: Add session state members on `SlintSystem`**

In `slint_system.h` (private section near `m_applying_inspector_sync`):

```cpp
  enum class InspectorRotationEditMode : uint8_t { euler = 0, quaternion = 1 };
  enum class InspectorMultiEditMode : uint8_t { absolute = 0, delta = 1 };

  InspectorRotationEditMode m_inspector_rotation_mode{
      InspectorRotationEditMode::euler};
  InspectorMultiEditMode m_inspector_multi_edit_mode{
      InspectorMultiEditMode::absolute};
  bool m_inspector_scale_link{true};
  /// -1 = none; else field id while keyboard draft is active.
  int m_inspector_focused_field{-1};
```

- [ ] **Step 2: Rewrite `syncInspectorFromSelection`**

Behavior:

1. If no selection → clear has-selection; return.
2. `ids = selection->getSelectedIds()`; primary = `getPrimarySelection()`.
3. Set `multi-edit-visible = ids.size() > 1`.
4. Push session toggles: rotation mode, scale link, multi-edit Absolute/Delta.
5. For each of pos/rot/scale components:
   - Gather values from all selected entities (rotation: Euler via `SceneSerializer::rotationToEulerDegrees`, or quat components if Quaternion mode and single-select).
   - If `ids.size()==1` or all equal → set float + `mixed=false`.
   - If Absolute multi and mixed → set `mixed=true` (placeholder).
   - If Delta multi → display `0` for editable numeric fields (`mixed=false`); entity label like `"3 selected"`.
6. Skip writing any property whose field id equals `m_inspector_focused_field`.
7. Keep `m_applying_inspector_sync` guard.

- [ ] **Step 3: Rewrite `applyInspectorTransform`**

Read UI floats + session mode. For each selected entity:

**Position / Scale (per edited component):**

- Single select, or Absolute multi: write absolute component. With Scale link: for each entity `setScale(applyScaleLinkEdit(old, axis, V))` so that entity’s edited axis becomes `V` and others keep proportion.
- Delta multi: `applyDeltaComponent`. With Scale link: `applyScaleLinkEdit(old, axis, old[axis]+delta)` per entity.

**Rotation:**

- Single + Euler: full Euler from UI → `SceneSerializer::rotationFromEulerDegrees` → `setRotation`.
- Single + Quaternion: build from UI x,y,z,w → `normalizeQuaternion` → `setRotation`.
- Multi: always `applyEulerDeltaDegrees(entity.rotation, delta_euler)` from Euler delta fields. While `multi-edit-visible`, UI shows Euler delta controls only (hide Quaternion fields).

After writes: `scene->markTransformsDirty()`, `editor_scene_edit->markDirty()`, `notifyViewportAfterInspectorTransformEdit(...)`.

- [ ] **Step 4: Bind Slint callbacks in existing binder**

Where `inspector-transform-edited` enqueues `UiEventKind::inspectorTransformEdited`, also connect:

- `field-focus-changed` → set/clear `m_inspector_focused_field`
- Esc while focused → clear focus id + `syncInspectorFromSelection` (discard draft)
- Mode toggles → update `m_inspector_*` members only
- Scrub/slider/accepted → enqueue `inspectorTransformEdited`

Reuse `m_applying_inspector_sync` so gizmo sync does not stomp focused drafts.

- [ ] **Step 5: Update floating Inspector snapshot**

Extend `NativeFloatPanelSnapshot` with new floats/bools and set them wherever `set_inspector_pos_x` is pushed in `dock_floating_window_host.cpp`.

- [ ] **Step 6: Build + smoke**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
./build/vs2026-debug/engine/src/editor/Debug/engine_editor.exe
```

Manual checks:

1. Single select: type Position X, Enter → mesh moves; scrub X → live move.
2. Rotation slider → live rotate; switch Quaternion → edit w → normalizes.
3. Scale link on: edit X → Y/Z follow ratios.
4. Multi-select Absolute: mixed `—`, set X=5 → all get 5.
5. Multi-select Delta: fields 0, set X=1 → each moves +1.
6. During type-in, gizmo drag does not overwrite the focused field text; Esc restores.

- [ ] **Step 7: Commit**

```powershell
git add engine/src/runtime/function/slint/slint_system.h `
  engine/src/runtime/function/slint/slint_system.cpp `
  engine/src/runtime/function/ui/docking/dock_floating_window_host.h `
  engine/src/runtime/function/ui/docking/dock_floating_window_host.cpp
git commit -m "$(cat <<'EOF'
feat: wire multi-select Inspector Transform apply and focus lock

EOF
)"
```

---

### Task 5: Docs + OpenSpec tasks checkoff + validate

**Files:**
- Modify: `openspec/changes/inspector-transform-godot-style/tasks.md` (mark items done)

- [ ] **Step 1: Re-run unit test + build**

```powershell
ctest --test-dir build/vs2026-debug -C Debug -R inspector_transform_ops_test --output-on-failure
cmake --build build/vs2026-debug --config Debug --target engine_editor
```

- [ ] **Step 2: Mark OpenSpec tasks complete; archive when workflow requires**

```powershell
openspec archive inspector-transform-godot-style
```

(Only archive post-merge if that is the project norm; otherwise leave the change folder open.)

- [ ] **Step 3: Final commit if docs/tasks changed**

```powershell
git add openspec/changes/inspector-transform-godot-style
git commit -m "$(cat <<'EOF'
docs: complete inspector-transform-godot-style tasks

EOF
)"
```

---

## Self-review

**Spec coverage**

| Requirement | Task |
|-------------|------|
| Horizontal axis fields + units + axis colors | Task 3 |
| Typed commit + scrub live | Task 3–4 |
| Rotation under-sliders (Euler) | Task 3 |
| Scale link proportional | Task 2 + 4 |
| Rotation Edit Mode Euler/Quaternion + normalize | Task 2 + 4 |
| Local only | Task 4 (no world path) |
| Multi Absolute/Delta + mixed + Rotation delta-only | Task 2 + 4 |
| Focus lock + Esc | Task 4 |
| No Undo / no Rotation Order | Out of scope (documented) |
| ADRs + glossary | Done pre-plan + Task 1 |

**Placeholder scan:** None intentional. Slint focus/`LineEdit` APIs may need small fork-specific adjustments at Task 3 compile time.

**Type consistency:** Ops use `Vec3`/`Quat`/`SceneSerializer` euler; Slint uses floats + bools; field ids are integers shared between Slint callbacks and `m_inspector_focused_field`.

**Euler note:** Grilling used “YXZ” as Godot UX shorthand; implementation locks to **SceneSerializer** `qz*qy*qx` (ADR 0001 / `CONTEXT.md`) so Inspector does not fork from scene assets.
