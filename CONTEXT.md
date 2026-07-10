# Blunder Engine

Editor and runtime for a Z-up, glTF-aligned 3D engine with Blender-inspired viewport gizmos.

## Language

### Navigate gizmo

**Positive ball**:
An axis endpoint handle on the positive side of a world axis, drawn solid in the axis color with an axis letter.
_Avoid_: Positive sphere, +handle

**Negative ball**:
An axis endpoint handle on the negative side of a world axis. Fill is a fixed neutral gray biased toward the axis color; border is the full axis color. Opaque so it can occlude arms and other balls when in front.
_Avoid_: Negative sphere, −handle, hollow ring

**Axis color**:
The full theme RGB for a world axis (X/Y/Z), used for positive balls, arms, and negative-ball borders.
_Avoid_: Primary color, main-axis color

**Muted axis fill**:
Negative-ball interior color: mix of fixed neutral gray `(0.20, 0.20, 0.20)` and axis color at mix factor `0.25`.
_Avoid_: Semi-transparent fill, view-background mix

### Transform gizmo (translate)

**Translate modal session**:
The shared interaction session for moving a selection in translate mode, whether started from a handle or from grab. Owns motion, constraints, confirm/cancel, and drag feedback.
_Avoid_: Gizmo drag only, ad-hoc translate handler

**Grab entry**:
Starting a Translate Modal Session with `G` for free view-plane translation. Confirms with LMB click (not release). Distinct from handle entry.
_Avoid_: Gizmo drag, handle press

**Constraint guide**:
Axis-colored lines through the drag-start pivot showing the active translation constraint. One line for a single axis, two for a plane, none for free center move. Guides stay pinned at drag-start while the object moves.
_Avoid_: Helpline, rubber-band, axis ray

**Drag ghost**:
A semi-transparent copy of only the active translate handle, drawn at the drag-start pose for the duration of the session.
_Avoid_: Full gizmo ghost, afterimage

**Origin dot**:
A small colored disc at the root of the active axis arrow (or on the active plane handle) during constrained translate drag. Single-axis drags use that axis color; plane drags use the plane's normal-axis color.
_Avoid_: Center disc, pivot bead (when meaning the always-visible free-move center)

**Transform-active outline**:
The temporary object outline color used while a translate modal session is active; a fixed light/off-white distinct from the normal selection outline. Restored when the session ends.
_Avoid_: Drag highlight, selection glow

**Reference axis arrows**:
The three axis arrows kept visible during translate drag as orientation reference. They follow the object's current position. Plane handles and the free-move center are hidden according to which handle is active.
_Avoid_: Inactive gizmos, leftover handles

**Constraint orientation cycle**:
Re-pressing the same axis or plane key during a translate modal session cycles constraint orientation: global → local → free. A different key starts a new global constraint for that axis or plane.
_Avoid_: Toggle mode, one-shot constrain

**MMB axis pick**:
Middle-mouse drag during an active translate modal session picks the nearest projected world axis from the mouse delta and commits a single-axis constraint on release.
_Avoid_: Orbit camera, right-click axis select

**Numeric input**:
Typed digits, minus, and decimal during a translate modal session set a signed distance along the active constraint; pointer motion is ignored while numeric input is active.
_Avoid_: Inspector field, typed offset in free mode
