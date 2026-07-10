## Context

P1 (`translate-modal-handle-parity`) introduced `TranslateModalSession` with handle entry, screen-space motion, and drag feedback. P2 (`translate-modal-grab`) added grab entry (`G`) with click-to-confirm. Constraints are still fixed at `beginFromHandle` / `beginFromGrab` — there is no mid-session keyboard constraint, MMB axis pick, or numeric distance input.

Grilling locked P3 as the full Blender-like constraint layer on the **same** session (handle and grab). Blender reference: `transform_event_modal_constraint`, `selectConstraint` / `setNearestAxis3d`, and modal `NumInput` in `E:/Dev/Blender`.

Domain terms: extend `CONTEXT.md` → “Transform gizmo (translate)” with constraint orientation, MMB axis pick, and numeric input.

## Goals / Non-Goals

**Goals:**

- Mid-session constraint switching for handle **and** grab sessions:
  - `X` / `Y` / `Z` → single-axis constraint
  - Re-press same axis key → cycle constraint orientation **global → local → free** (clear constraint)
  - Different axis key → switch to that axis and reset the orientation cycle to global
  - `Shift+X` / `Shift+Y` / `Shift+Z` → plane constraint locking that axis out (YZ / ZX / XY), with the same re-press orientation cycle
- MMB axis pick while a session is active: hold MMB, mouse delta picks nearest projected axis, release commits that single-axis constraint
- Numeric distance input while a session is active: digits / `-` / `.` build a value applied along the active constraint (free → view-plane distance semantics); confirm keeps it; editing keys follow Blender-like modal numinput
- Feedback updates when the live constraint changes (guides, origin dot, plane/center visibility); cursor + transform-active outline unchanged
- `Enter` confirms an active Translate Modal Session (both entry kinds), matching the locked confirm set

**Non-Goals:**

- Rotate / scale modals (`R` / `S`)
- Snap increments, precision mode, proportional edit
- Full Blender orientation stack (View / Cursor / Custom / Gimbal) — only **global** and **local**
- Changing idle gizmo look or P1/P2 confirm policies for LMB (handles stay release-to-confirm; grab stays click-to-confirm)
- Status-bar / header constraint text UI (optional later; not required for P3)

## Decisions

### 1. Mutable constraint + orientation on the existing session

**Decision:** Extend `TranslateModalSession` with a live constraint descriptor:

| Field | Meaning |
|-------|---------|
| Constraint kind | free / axis X\|Y\|Z / plane XY\|YZ\|ZX |
| Orientation | global or local (ignored when free) |
| Orientation cycle step | tracks re-press state for the current axis/plane key |

API surface (names indicative): `applyAxisConstraintKey`, `applyPlaneConstraintKey`, `beginMmbAxisPick` / `updateMmbAxisPick` / `endMmbAxisPick`, `appendNumericChar` / `clearNumeric` / `hasNumericInput`, plus existing `onPointerMove` / `confirm` / `cancel`.

After any constraint or numeric change, recompute position from **session-start TRS** + current pointer (or numeric value), never from the previous constrained delta alone.

**Alternatives:** Fork a `ConstraintController` outside the session. Rejected — P1/P2 architecture exists so motion/feedback stay single-owned.

### 2. Keyboard cycle: global → local → free

**Decision:** Simplified Blender orientation cycle for our two spaces:

1. First press of an axis/plane key (or press of a **different** key) → constrain in **global**
2. Re-press the **same** key → switch to **local** (entity rotation at session start)
3. Re-press again → **free** (clear constraint; grab-like view-plane motion)

`Shift+X` locks X (plane YZ); `Shift+Y` → ZX; `Shift+Z` → XY. Plane keys share the same three-step cycle independently of single-axis keys (pressing `X` after `Shift+X` is a “different” constraint and resets to global X).

Local axes use the entity’s session-start rotation (same basis source as gizmo local space). Global uses world X/Y/Z.

**Alternatives:** Full Blender multi-slot orientation array. Rejected for P3 scope. Toggle only global↔local without free-on-third-press. Rejected — third press clearing constraint is the familiar Blender escape hatch.

### 3. Motion re-projection after constraint change

**Decision:** Keep the original pointer-start and object-start. On constraint change, rebuild the constraint basis (global or local), then run the existing screen-delta → constrain pipeline so the object jumps onto the new constraint from the current pointer — Blender-like, not “accumulate in old space then project.”

Numeric mode: when a parsed value is active, pointer motion does **not** override the numeric distance; clearing numeric (Backspace to empty, or Escape clearing numinput first) returns to pointer-driven motion.

### 4. MMB axis pick

**Decision:** While a Translate Modal Session is active:

| Phase | Behavior |
|-------|----------|
| MMB press | Enter axis-pick mode; do not orbit camera (session already locks camera) |
| MMB hold + move | Choose nearest of the three **current orientation** axes by projecting axis directions to screen and comparing to mouse delta from pick start (Blender `setNearestAxis3d` analogue) |
| MMB release | Commit single-axis constraint in the current orientation cycle’s active orientation (default global if free); exit pick mode |

Visual during pick: show three axis guides (or highlight nearest) through drag-start pivot; on commit, fall back to normal single-axis guide feedback.

**Alternatives:** MMB click without drag. Rejected — Blender needs delta to disambiguate. Plane pick via modifier during MMB. Deferred — P3 is axis pick only.

### 5. Numeric input

**Decision:** Minimal Blender-like modal numinput for **one** distance value:

- First digit, `-`, or `.` while session active starts numeric editing (consume the key; do not trigger unrelated shortcuts)
- Subsequent digits / one `.` / leading `-` edit the buffer
- Backspace edits; empty buffer exits numeric mode and restores pointer motion
- Escape: if numeric active → clear numeric first; else cancel session (existing)
- Value meaning: signed distance along the active single axis; for plane/free, distance along the pointer’s current constrained/view-plane direction unit (document in tests with fixed cases)
- Confirm (LMB per entry policy, or Enter) applies current numeric (or pointer) result and ends session

No unit suffixes, no multi-axis `Tab` between components in P3.

**Alternatives:** Full Blender `NumInput` port. Rejected — too large; one scalar is enough for translate distance.

### 6. Feedback when constraint changes

**Decision:**

| Feedback | Rule |
|----------|------|
| Guides | Follow **live** constraint (0/1/2 lines); pinned at drag-start |
| Origin dot | Follow live constraint (same P1 color rules); none when free |
| Visibility | Live constraint drives plane/center hide rules; reference arrows always on |
| Handle ghost | Handle entry: keep ghost of **originally pressed** handle; grab: never show handle ghost |
| Cursor / outline | Unchanged for session lifetime |

When grab goes free → constrained, guides and origin dot appear; planes hide per axis/plane rules.

### 7. Input routing ownership

**Decision:** While `TranslateModalSession` is active, `TransformGizmoController::onKeyPressed` (and mouse path) owns:

- `X`/`Y`/`Z` (+ Shift) constraint keys
- Digit / `-` / `.` / Backspace numeric
- `Enter` confirm
- Escape (numeric-clear then cancel)
- MMB axis pick

Do not let idle mode shortcuts (`G`/`R`/`S`) or camera orbit steal these while the session is active. Reuse existing “editor 3D shortcuts enabled / no text-field focus” gating.

## Risks / Trade-offs

- **[Risk] Local vs global basis bugs on oblique views** → Mitigation: unit tests with known entity rotation + camera; manual QA after each cycle step.
- **[Risk] Numeric vs pointer fight** → Mitigation: explicit numeric-active flag; pointer ignored until numeric cleared.
- **[Risk] MMB conflicts with future pan bindings** → Mitigation: only while translate modal session active; camera already interaction-locked.
- **[Risk] Escape double meaning (numinput vs cancel)** → Mitigation: document and test ordered handling; match Blender.
- **[Trade-off] Only global/local orientations** → Accepted; full orientation stack is a later change.
- **[Trade-off] No header constraint text** → Accepted for P3; guides + motion are the primary feedback.

## Migration Plan

- Editor-only; no asset migration.
- Requires P1 session/feedback and P2 grab entry on the branch.
- Rollback: remove mid-session key/MMB/numeric handlers; session begin constraints unchanged.

## Open Questions

- Exact free/plane numeric direction unit when unconstrained (use last pointer view-plane delta direction vs camera right) — pick during implement with a fixed test vector.
- Whether MMB pick uses global axes only or respects current orientation step — default: use **global** axes for pick projection unless a constraint orientation is already local.
