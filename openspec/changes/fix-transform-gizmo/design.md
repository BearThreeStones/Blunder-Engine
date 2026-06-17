## Context

The transform gizmo is implemented as a Vulkan screen-space overlay (`TransformGizmoOverlay`) driven by `TransformGizmoController`, with procedural geometry in `transform_gizmo.slang`. Modes are `translate`, `rotate`, `scale`, and `none`. Slint toolbar and keyboard shortcuts (G/R/S) call `RenderSystem::setTransformGizmoMode`.

Known issues from investigation:

1. `EditorSelectionSystem::setSelection` unconditionally calls `setTransformGizmoMode(translate)` when a valid entity is selected.
2. Translate mode draws plane handles (`GizmoDrawStyle::plane`) but axis arrows (`GizmoDrawStyle::arrow`) do not appear—likely a shader/matrix issue isolated to the arrow path.
3. Scale mode reuses translate arrow drawing and has no pick/drag implementation (`onMousePressed` returns early for scale).
4. Rotate mode has dial drawing and interaction implemented; may appear broken when mode is reset or when arrows/dials are confused with translate planes.

Default controller mode is `none` until the user activates a mode via toolbar or hotkey.

## Goals / Non-Goals

**Goals:**

- Preserve the active transform gizmo mode when selection changes (option A — Blender-style).
- Restore visible, pickable translate axis arrows alongside existing plane handles.
- Deliver a functional scale gizmo with distinct visuals, picking, and drag behavior.
- Verify rotate dial rendering after P0/P1; fix only if still broken.

**Non-Goals:**

- Gizmo size preferences UI, snapping, numeric input integration, or multi-selection.
- Screen-space constant-width lines (future polish; use existing Blender-matched local-space thickness for now).
- Changing global/local space toggle behavior or navigate gizmo.

## Decisions

### 1. Selection must not reset gizmo mode (P0)

**Decision:** Remove `setTransformGizmoMode(translate)` from `EditorSelectionSystem::setSelection`. Keep `requestViewportRedraw()` on selection change so the gizmo repositions to the new pivot.

**Alternatives considered:**

- **B — default translate only on first selection:** Rejected; user chose A.
- **Set translate when mode is `none`:** Optional follow-up; not required for A. First-time users still use toolbar/G/R/S.

### 2. Arrow fix strategy (P1)

**Decision:** Debug and fix the existing `vsArrow` path before adding new geometry types.

Investigation order:

1. Confirm `drawTranslateHandle` is invoked for `trans_x/y/z` (not skipped by fade).
2. Validate `gizmoHandleMatrix` produces non-degenerate bases for all three axes (watch parallel-axis cross products).
3. Inspect `vsArrow` vertex generation and `TransformGizmoDrawCounts::k_arrow_verts` parity with shader constants.
4. If local-space thickness is sub-pixel, bump `k_axis_line_width` or minimum `lineWidthScale()` floor for arrows only.

**Alternatives:**

- Replace arrows with line-list overlay pass: larger refactor; defer unless shader fix fails.

### 3. Scale gizmo visuals and interaction (P2)

**Decision:** Add `GizmoDrawStyle::scale_cube` (or `box`) in `transform_gizmo.slang`—short box handles on each axis plus uniform center cube—mirroring Blender's scale gizmo silhouette at a simplified level.

- Extend `ManipulatorAxis` usage: reuse `trans_x/y/z` for axis scale, `trans_c` for uniform scale.
- Add `pickScaleGizmoHandle` in `transform_gizmo_pick.cpp` (box ray intersection, reuse group scale).
- Extend `TransformGizmoController::onMousePressed` / `onMouseMoved` / `onMouseReleased` for scale drag; apply delta to `Entity::setScale` with axis masking (same masks as translation).
- Scale overlay branch in `recordGizmoDraw` uses new style instead of `drawTranslateHandle` arrows.

**Alternatives:**

- Reuse arrows for scale: insufficient visual distinction and already broken for translate.

### 4. Rotate verification

**Decision:** After P0 and P1, manually verify rotate dials in perspective and orthographic views. No code change unless dials still missing—then treat as dial shader/matrix bug similar to arrows.

## Risks / Trade-offs

- **[Risk] Preserving `none` mode across selection** → User selects entity but sees no gizmo until pressing G/R/S. **Mitigation:** Acceptable per option A; matches Blender when transform isn't active.
- **[Risk] Arrow root cause unclear until runtime debug** → P1 may take iteration. **Mitigation:** Add temporary debug draw or unit test for `gizmoHandleMatrix` orthogonality.
- **[Risk] Scale drag with parent hierarchy** → Local vs global scale semantics differ from Blender. **Mitigation:** Match existing rotation pattern—scale in local space for local gizmo, component-wise in parent space for global (document in code comments).
- **[Risk] `k_max_gizmo_draws_per_frame` (12)** → Scale + translate paths must stay under limit. **Mitigation:** Scale uses 4 handles; translate uses 7; rotate uses 3–4. No change needed.

## Migration Plan

Single PR, no data migration. Validate manually:

1. Select entity A → Rotate → select entity B → still Rotate, dials on B.
2. Translate shows planes + arrows.
3. Scale drags update inspector scale fields.
4. Escape cancels drag; toolbar sync stays correct.

Rollback: revert commits; no persistent state.

## Open Questions

- Should global scale mode scale world axes or parent-relative components? **Default:** follow `GizmoSpace` the same way rotation does; refine if artist feedback says otherwise.
