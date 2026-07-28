# Camera Gizmo (Blender-like) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Edit Mode shows and interacts with scene **Camera Component** entities via a Blender-like **Camera Gizmo** (draw, pick, FOV/clip handles, Align View/Camera), without showing that chrome in the Player.

**Architecture:** Add `CameraGizmoOverlay` to `OverlaySystem`, gated by `editorOverlaysEnabled` / authorship overlays. Pure geometry helpers build view-frame + up-triangle from FOV + viewport aspect + fixed display distance. Hit-test runs before mesh pick. Pose stays on existing Transform gizmo. FOV/clip and Align Camera to View seal Document History at interaction boundaries. Align target resolve reuses Play’s Main-then-first rule.

**Tech Stack:** C++20, OverlayLinePass / screen overlay patterns, `SceneInstance` cameras, Document History Commands, Slint/editor menu shortcuts, CTest.

## Global Constraints

- Glossary: `CONTEXT.md` — **Camera Gizmo**, **View frame**, **Up triangle**, **Align View to Camera**, **Align Camera to View**, **Camera Component**, **Main Camera**, **Editor Overlay**, **Editor Camera**.
- Visual: Blender wire (origin, four edges, view frame, up triangle); muted unselected; selection color when single-selected.
- Aspect = editor viewport; frame depth = fixed local display distance (no sensor aspect field).
- Full interaction this slice: pick (before mesh), Transform pose, FOV/clip drag, Align both ways.
- Align copies TRS + vertical FOV only (not near/far).
- Align target: single Camera selection; multi-select invalid; no selection → Main else first (stable EntityId); none → fail.
- History: FOV/clip on release; Align Camera to View yes; Align View to Camera no.
- Shortcuts: menu + Numpad 0 / Ctrl+Alt+Numpad 0 + laptop fallbacks.
- Player: never draw/interact (Editor Overlay gate).
- TDD for geometry + Align target resolve + history seal helpers; frequent scoped commits.
- OpenSpec change: `camera-gizmo`.

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/function/render/overlay/camera_gizmo_geometry.h` | Pure: frame corners, up triangle, frustum rays |
| `engine/src/runtime/function/render/overlay/camera_gizmo_overlay.h/.cpp` | Draw + hover/hit + FOV/clip drag session |
| `engine/src/runtime/function/render/overlay/overlay_system.h/.cpp` | Own/sync/draw Camera Gizmo |
| `engine/src/runtime/function/render/render_system.cpp` | Event order: Camera Gizmo before mesh pick |
| `engine/src/runtime/function/scene/align_camera_target.h` | Resolve Align target (selection / Main / first) |
| `engine/src/runtime/function/editor/...` (history command) | Camera param + Align Camera Commands |
| `engine/src/runtime/function/slint/...` / UI host | Menu + shortcuts |
| `engine/src/tests/camera_gizmo_geometry_test.cpp` | Geometry TDD |
| `engine/src/tests/align_camera_target_test.cpp` | Target resolve TDD |
| `openspec/changes/camera-gizmo/` | Specs / tasks |

```
Edit Mode viewport                         Player
─────────────────                          ──────
Camera Gizmo draw ON                       OFF (editorOverlaysEnabled)
pick Camera before mesh                    no Camera Gizmo input
Transform gizmo for pose                   —
FOV/clip handles (single select)           —
Align View / Align Camera                  —
```

---

### Task 0 (optional): Branch

- [ ] **Step 1:** `git checkout -b feat/camera-gizmo` from current integration tip
- [ ] **Step 2:** Commit OpenSpec scaffold if not already committed

---

### Task 1: View-frame geometry (TDD)

**Files:**
- Create: `engine/src/runtime/function/render/overlay/camera_gizmo_geometry.h`
- Create: `engine/src/tests/camera_gizmo_geometry_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt`

**Interfaces:**

```cpp
namespace Blunder {
struct CameraGizmoFrame {
  Vec3 origin;
  Vec3 corners[4]; // TL, TR, BR, BL in world or local — document in header
  Vec3 up_triangle[3];
};

/// Local-space frame at +display_distance along camera look (-Z glTF),
/// sized by vertical_fov_radians and aspect (width/height).
CameraGizmoFrame buildCameraGizmoFrameLocal(
    float vertical_fov_radians, float aspect, float display_distance);
}
```

Constants: `kCameraGizmoDisplayDistance` (e.g. `1.0f`) in header.

- [ ] **Step 1: Failing tests** — aspect 16:9 wider than tall; FOV larger → larger frame; up triangle above top edge
- [ ] **Step 2: Implement header; PASS**
- [ ] **Step 3: Commit** `feat(overlay): Camera Gizmo view-frame geometry helpers`

---

### Task 2: Draw overlay in OverlaySystem

**Files:**
- Create: `camera_gizmo_overlay.h/.cpp`
- Modify: `overlay_system.h/.cpp`
- Mirror line-draw pattern from axes/origins or Transform line submission

**Behavior:**
- `forEachCamera` / entity iteration: build world matrices × local frame
- Muted color vs selection color (use existing selection theme if available)
- Call only when `authorshipOverlaysActive()` / `editorOverlaysEnabled`

- [ ] **Step 1: Stub overlay + register sync/draw**
- [ ] **Step 2: Draw all cameras**
- [ ] **Step 3: Build `engine_editor`; commit** `feat(overlay): draw Camera Gizmo for scene cameras`

---

### Task 3: Pick before mesh

**Files:**
- Modify: `camera_gizmo_overlay` hit-test (screen dist to segments / frame)
- Modify: `render_system.cpp` `onEvent` — after Transform/Navigate if those already win, **before** `m_viewport_pick`

**Behavior:**
- Click body/frame → select that entity (replace selection)
- Player / `!editorOverlaysEnabled` → skip

- [ ] **Step 1: Hit-test helpers + unit or focused test if cheap**
- [ ] **Step 2: Wire event order**
- [ ] **Step 3: Commit** `feat(overlay): Camera Gizmo pick before mesh`

---

### Task 4: FOV / clip handles + history

**Files:**
- Modify: `camera_gizmo_overlay` drag session
- Create/Modify: Document History Command for camera params (mirror Transform commit boundary)
- Modify: Inspector apply path to seal same Command on FOV/near/far commit if easy

**Behavior:**
- Handles only when exactly one selected Camera entity
- Drag FOV via view-frame edge (Blender-like); near/far via clip plane handles
- Live write `CameraComponent`; on release → history Command
- Multi-select: no handles

- [ ] **Step 1: Drag session + live update**
- [ ] **Step 2: History seal on release**
- [ ] **Step 3: Commit** `feat(overlay): Camera Gizmo FOV and clip handles with history`

---

### Task 5: Align commands + shortcuts

**Files:**
- Create: `align_camera_target.h` (+ test)
- Implement Align View / Align Camera actions (editor service / UI host)
- Menu + shortcuts: Numpad0, Ctrl+Alt+Numpad0, laptop fallbacks (e.g. Alt+Shift+0 / Ctrl+Alt+Shift+0 — confirm no conflict at impl time)

**Interfaces:**

```cpp
struct AlignCameraTargetResult {
  bool ok{false};
  EntityId entity_id{k_invalid_entity_id};
  // optional error tag for UI
};

AlignCameraTargetResult resolveAlignCameraTarget(
    const SceneInstance& scene,
    /* selection view: single / multi / none */);
```

- Align View: set Editor Camera look-at from entity world + FOV; **no** history
- Align Camera: set entity world TRS + FOV from Editor Camera; **history**
- Multi-select → fail; none + no cameras → fail

- [ ] **Step 1: TDD resolveAlignCameraTarget**
- [ ] **Step 2: Implement both Align actions**
- [ ] **Step 3: Menu + shortcuts**
- [ ] **Step 4: Commit** `feat(editor): Align View to Camera and Align Camera to View`

---

### Task 6: Docs + USER-VERIFY

- [ ] **Step 1:** Confirm `CONTEXT.md` matches behavior (already drafted in grill)
- [ ] **Step 2:** Manual checklist
  1. Two cameras: both wires visible; select one → selection color + handles
  2. Click camera over mesh → selects camera
  3. Drag FOV/clip → undo restores
  4. Align View / Align Camera (selected + unselected Main/first)
  5. Player: no Camera Gizmo
- [ ] **Step 3:** Mark OpenSpec tasks; leave 6.2 USER-VERIFY until human confirms

---

## Self-review

| Requirement | Task |
|-------------|------|
| Blender draw | 1–2 |
| Pick priority | 3 |
| FOV/clip + history | 4 |
| Align + shortcuts | 5 |
| Player off | 2–3 (overlay gate) |
| Glossary | 6 |

**Out of scope:** continuous Main lock, sensor aspect field, multi-select FOV batch, ortho Camera gizmo.

---

## Execution Handoff

Plan: `docs/superpowers/plans/2026-07-28-camera-gizmo.md`  
OpenSpec: `openspec/changes/camera-gizmo/`

**Next:** `/opsx:apply` or subagent-driven-development on `feat/camera-gizmo`.
