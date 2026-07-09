## Why

P1 landed Blender-like translate **handle** motion and drag feedback through a shared Translate Modal Session. Users still cannot start free translate with `G` (grab). Without grab entry, keyboard-first object placement remains incomplete, and P3 constraint shortcuts have no modal to attach to.

## What Changes

- Add **grab entry** into the existing Translate Modal Session: pressing `G` (with a selection) begins a free view-plane translate session via `beginFromGrab` (or equivalent).
- Grab confirm/cancel differs from handles: **LMB click confirms** (click-to-confirm); RMB / Escape cancels and restores drag-start TRS.
- Reuse P1 motion (free/center path), cursor, transform-active outline, inspector sync, and viewport redraw while the grab session is active.
- Grab-session feedback: no constraint guides (free move); no active-handle drag ghost (no handle was pressed); reference gizmo visibility follows a grab-specific rule (hide solid center / keep or hide idle handles — decided in design).
- **Out of P2:** keyboard constraints (X/Y/Z, Shift-plane, re-press space), MMB axis pick, numeric input (P3); rotate/scale grab (`R`/`S`).

## Capabilities

### New Capabilities

- _(none)_ — grab is an entry path on the existing session capability.

### Modified Capabilities

- `translate-modal-session`: Add grab entry (`G`), click-to-confirm for grab-started sessions, and grab-specific feedback rules while reusing screen-space free motion and shared cancel/outline/cursor behavior.
- `transform-gizmo`: Viewport/editor key routing must start a translate modal session on `G` when a selection exists (without requiring a handle hit).

## Impact

- `TranslateModalSession`: `beginFromGrab`, entry-kind (handle vs grab), confirm semantics per entry
- `TransformGizmoController` / editor key handling: `G` begins session; LMB click confirms when grab-active
- Overlay: grab-active visibility / no ghost / no guides
- Outline + cursor: already session-driven from P1 — ensure grab sets the same active flags
- Docs: `CONTEXT.md` grab term; short note in `docs/agents/render-pipeline.md`
- Depends on P1 (`translate-modal-handle-parity`) session + feedback infrastructure
- Blender reference: `E:/Dev/Blender` (`TRANSFORM_OT_translate`, grab keymap, release_confirm vs click confirm)
