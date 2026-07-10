## Why

P1 and P2 landed a shared Translate Modal Session for handle drag and `G` grab, but constraints are still fixed at session start (handle axis/plane/center, or free grab). Without mid-session keyboard constraints, MMB axis pick, and numeric distance input, keyboard-first Blender-like placement remains incomplete.

## What Changes

- Add **mid-session constraint switching** on an active Translate Modal Session (handle or grab):
  - `X` / `Y` / `Z` → single-axis constraint
  - Re-press the same axis key → toggle constraint orientation between **global** and **local** (Blender-like space cycle for that axis)
  - `Shift+X` / `Shift+Y` / `Shift+Z` → plane constraint excluding that axis (YZ / ZX / XY)
- Add **MMB axis pick**: while a session is active, middle-mouse drag selects a constraint axis (Blender-like), then continues the session under that constraint.
- Add **numeric input**: while a session is active, typing a number (with optional `-` / `.`) sets an exact translation distance along the active constraint; confirm applies it, Escape/backspace editing follows Blender-like modal numeric rules (design locks details).
- Update drag feedback when the active constraint changes: guides, origin dot, and handle visibility follow the new constraint (same P1 rules); grab free → constrained transitions start showing guides.
- **Out of P3:** rotate/scale modals (`R`/`S`), snap increments, precision/snapping modes beyond numeric distance, changing idle gizmo look.

## Capabilities

### New Capabilities

- _(none)_ — constraints, MMB pick, and numeric input extend the existing session capability.

### Modified Capabilities

- `translate-modal-session`: Mid-session keyboard constraints (axis / plane / global↔local re-press), MMB axis pick, numeric distance input, and feedback updates when the active constraint changes (handle and grab entries).
- `transform-gizmo`: While a Translate Modal Session is active, route `X`/`Y`/`Z` (+ Shift), MMB axis-pick gestures, and numeric key/text input into the session instead of mode shortcuts / camera / idle gizmo behavior.

## Impact

- `TranslateModalSession`: mutable constraint + orientation (global/local); constraint-change API; numeric buffer; re-project motion from session-start TRS after constraint/numeric changes
- `TransformGizmoController` / editor input: key routing while session active; MMB consume for axis pick; text/number events for numeric mode
- Overlay: guides / origin-dot / visibility react to live constraint (including grab that becomes constrained)
- Docs: `CONTEXT.md` terms for constraint orientation, MMB axis pick, numeric input; short note in `docs/agents/render-pipeline.md`
- Depends on P1 session/feedback and P2 grab entry (`translate-modal-handle-parity`, `translate-modal-grab`)
- Blender reference: `E:/Dev/Blender` (`TRANSFORM_OT_translate`, constraint keymap, MMB axis select, modal numinput)
