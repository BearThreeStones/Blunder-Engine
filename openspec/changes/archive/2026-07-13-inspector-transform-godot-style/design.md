## Context

Inspector Transform today is nine stacked `TransformSlider` rows in `inspector_panel.slint`, synced/applied by `SlintSystem::syncInspectorFromSelection` / `applyInspectorTransform` against the primary selection only. Entity rotation is already a Quaternion; Euler degrees go through `SceneSerializer` helpers. Gizmo edits already call `syncInspectorFromSelection` and force viewport redraw. Grilling + ADRs locked Local-only editing, Quaternion authority, Scale link ratios, Absolute/Delta multi-edit (Rotation delta-only), focus lock, and no Undo.

## Goals / Non-Goals

**Goals:**
- Godot-like Local Transform UI (layout/interaction) inside Blunder Inspector chrome
- Typed commit + live scrub; Euler under-sliders; Scale link; Euler|Quaternion mode
- Multi-select Absolute/Delta with mixed placeholders; Rotation multi-edit as Euler delta only
- Focused-field lock against external sync; Esc discards draft
- Testable pure ops in `inspector_transform_ops`

**Non-Goals:**
- Rotation Order dropdown / Basis mode / World Transform editor
- Undo/command stack
- Changing `SceneSerializer` on-disk euler convention
- Restyling Blinn-Phong / SSAO sections
- Pixel-perfect Godot theme skin

## Decisions

1. **Pure ops module vs inline SlintSystem math**  
   Extract `inspector_transform_ops` for scale-link, Absolute/Delta component apply, Euler delta on quat, quat normalize.  
   *Why:* Unit-testable without Slint; keeps `SlintSystem` as adapter.  
   *Alt:* All logic in `slint_system.cpp` — rejected (hard to test).

2. **Fixed Euler = SceneSerializer convention (`qz * qy * qx`)**  
   Inspector Euler view reuses existing helpers rather than introducing Godot Y-up YXZ math on Z-up axes.  
   *Why:* Matches scene assets and current Inspector; avoids dual conventions.  
   *Alt:* Literal Godot YXZ — rejected for this change (see ADR 0001 / CONTEXT.md).

3. **Session UI state on `SlintSystem` (not scene)**  
   Rotation mode, Scale link, Multi-edit Absolute/Delta persist for the editor run across selection changes; defaults Euler / link on / Absolute.  
   *Why:* Matches grilled UX; no asset churn.  
   *Alt:* Per-entity prefs or disk persistence — deferred.

4. **Multi-edit: Absolute/Delta for TRS; Rotation Absolute deferred**  
   Mixed components show `—` in Absolute; Delta fields baseline at `0`. Multi-select forces Euler delta controls (hide Quaternion fields).  
   *Why:* Absolute Euler/Quat multi-set destroys relative poses; ADR 0002.  
   *Alt:* Primary-only multi-edit — rejected by product decision.

5. **Hybrid commit**  
   Keyboard: Enter/blur commit; scrub + rotation slider: live via existing `inspectorTransformEdited` → `applyInspectorTransform`. Focus lock skips sync overwrite for the focused field id.  
   *Why:* Safe typing without losing live gizmo-adjacent feel.

6. **UI chrome**  
   New `AxisNumberField` + Transform section redesign; Blunder colors (`#ff3352` / `#8bdc00` / `#2890ff`); shading blocks untouched.

## Risks / Trade-offs

- [Slint LineEdit focus/scrub APIs differ by fork] → Mitigate: adjust against `docs/agents/slint-fork.md` at compile time; keep commit semantics fixed.
- [Euler decompose ambiguity / gimbal] → Mitigate: same helpers as scene I/O; accept known euler quirks; Quaternion mode for precise edits.
- [Scale link + Absolute multi surprises] → Mitigate: per-entity `applyScaleLinkEdit(old, axis, V)` so each object’s X becomes V while preserving that object’s ratios.
- [Floating Inspector snapshot drift] → Mitigate: extend `NativeFloatPanelSnapshot` alongside main window props.
- [No Undo] → Mitigate: documented; same as current gizmo path.

## Migration Plan

- No asset migration. Ship UI + apply path behind normal editor build.
- Rollback: revert change; old slider Inspector returns.
- After implement: `/opsx:apply` per `docs/superpowers/plans/2026-07-12-inspector-transform-godot-style.md`; archive when done.

## Open Questions

- None blocking. Slint focus/scrub wiring details resolved during Task 3 compile against the fork.
