## Why

The Inspector Transform section is still a stack of coarse XYZ sliders. Editors need Godot-like local TRS editing: typed axis fields, scrubbing, Scale link, Euler/Quaternion presentation, and multi-select Absolute/Delta apply — without changing how entities store transforms.

## What Changes

- Replace stacked Transform sliders with a Godot-like Local Transform UI (horizontal axis number fields, units, rotation under-sliders, collapsible header)
- Add typed commit + live scrub on axis fields; keep rotation under-sliders live
- Add Scale link (proportional multi-axis edit)
- Add Rotation Edit Mode: Euler | Quaternion (Quaternion remains authoritative storage; Quaternion edits normalize on commit)
- Add multi-select Multi-edit mode: Absolute | Delta (Rotation multi-edit is Delta-only via Euler delta)
- Add focused-field lock so gizmo sync does not clobber an in-progress typed draft
- Add pure `inspector_transform_ops` helpers + unit tests; wire through `SlintSystem` sync/apply
- No Undo, no Rotation Order dropdown, no World/Global Transform editor, no shading-panel restyle

## Capabilities

### New Capabilities
- `inspector-transform`: Local Transform Inspector presentation and edit semantics (single- and multi-select, Euler/Quaternion mode, Scale link, field commit/scrub, focus lock)

### Modified Capabilities
- *(none — no existing inspector-transform capability; viewport redraw-on-edit remains as already specified elsewhere)*

## Impact

- `engine/src/runtime/function/slint/inspector_panel.slint`, `axis_number_field.slint`, `editor_window.slint`, optional floating panel bindings
- `engine/src/runtime/function/slint/slint_system.cpp` / `.h` (`syncInspectorFromSelection`, `applyInspectorTransform`)
- New `engine/src/runtime/function/editor/inspector_transform_ops.*` + `engine/src/tests/inspector_transform_ops_test.cpp`
- `dock_floating_window_host` inspector snapshot fields if native floating Inspector is kept in sync
- Reuses `SceneSerializer` euler helpers for fixed engine Euler order; Entity TRS storage unchanged
- ADRs: `docs/adr/0001-inspector-authoritative-quaternion.md`, `docs/adr/0002-inspector-multi-edit-absolute-delta.md`
- Plan: `docs/superpowers/plans/2026-07-12-inspector-transform-godot-style.md`
