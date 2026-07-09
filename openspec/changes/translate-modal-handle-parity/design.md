## Context

Translate handle drag today lives entirely in `TransformGizmoController`: press picks a handle, `onMouseMoved` uses `dragWorldPoint` (ray–axis closest point; finite plane quads) and `worldTranslationDelta`. Feedback is limited to handle highlight + live inspector sync. Blender’s path is different: gizmo click invokes `TRANSFORM_OT_translate`, which maps screen mouse delta through `convertViewVec` / constraint projection and draws constraint guides, cursor, and gizmo modal draw state.

Grilling locked a three-phase delivery. P1 is **handle entry only**, but the architecture must be a reusable **Translate Modal Session** so P2 (`G` grab) and P3 (full constraints / MMB axis pick / numeric input) do not fork behavior.

Domain terms live in `CONTEXT.md` under “Transform gizmo (translate)”.

## Goals / Non-Goals

**Goals:**

- Screen-space translate motion for axis / infinite plane / free center (Blender-like), including view-aligned axis fallback.
- P1 drag feedback: cursor, constraint guides, drag ghost, origin dot, handle visibility, transform-active outline.
- Handle confirm/cancel: LMB release confirms; RMB / Escape cancels and restores drag-start TRS.
- Session API that P2 can enter without rewriting motion/feedback.

**Non-Goals (P1):**

- Grab (`G`) modal entry, keyboard constraints, MMB axis pick, numeric input.
- Rotate / scale modal parity.
- Changing idle translate gizmo look (hover already exists).
- Replacing the whole transform operator stack with a Blender-faithful modal for all modes.

## Decisions

### 1. Translate Modal Session as the single owner of drag state

**Decision:** Add a dedicated session type (e.g. `TranslateModalSession`) owned by / used from `TransformGizmoController`. While active it owns: constraint kind, drag-start entity TRS + pivot, motion mapping, feedback flags, and end reasons (confirm/cancel).

**Alternatives:** Keep expanding `TransformGizmoController` ad hoc; duplicate grab later. Rejected — grilling chose a shared session to avoid path divergence.

### 2. Motion: screen delta → world, then constrain

**Decision:** On each move, convert mouse delta to a view-plane world delta (Blender `ED_view3d_win_to_delta` / `convertViewVec` analogue using `EditorCamera`), accumulate from drag-start, then apply:

| Handle | Constraint |
|--------|------------|
| Axis X/Y/Z | Project onto that axis (`axisProjection` analogue; view-aligned fallback when axis ≈ view) |
| Plane XY/YZ/ZX | Intersect / project onto **infinite** constraint plane |
| Center | Unconstrained view-plane delta |

**Alternatives:** Keep ray–line closest point. Rejected — root cause of sticky feel; finite plane quads drop tracking.

### 3. Feedback split: guides at drag-start, reference arrows follow object

**Decision:**

- **Constraint guides** drawn through **drag-start** pivot (pinned).
- **Reference axis arrows** follow the entity’s **current** world pivot.
- **Drag ghost** = semi-transparent copy of **active handle only** at drag-start pose.
- **Center disc** hidden while session active; center drag still shows center ghost at start.
- **Planes:** axis drag hides all planes; plane drag keeps the active plane, hides other planes.
- **Origin dot:** axis → root of active axis arrow (axis color); plane → on visible active plane handle (normal-axis color).

### 4. Cursor and outline

**Decision:**

- Set four-way move cursor for the session duration; restore on end (hover does not change cursor).
- Drive outline via existing transform-drag outline hook, but P1 color is fixed **light/off-white** (product choice; updates `outline-drag-color` requirement). Session active ⇒ transform-active outline; end ⇒ selection orange.

### 5. Confirm / cancel (handle path)

**Decision:** LMB release → confirm (keep current TRS). RMB or Escape → cancel (restore `entity` TRS from session start) and end session. Do not require a second LMB click for handles (P2 grab will use click-to-confirm).

### 6. Phased delivery stays in one change’s design, P1 tasks only

**Decision:** This change’s `tasks.md` implements **P1 only**. Design documents P2/P3 hooks (session `beginFromGrab`, constraint keymap) as non-goals for coding now so later changes extend the same type.

## Risks / Trade-offs

- **[Risk] Screen-delta calibration differs from Blender depth handling** → Mitigation: match `EditorCamera` ray/projection helpers; add unit tests for axis/plane/center deltas at known camera poses; manual QA on oblique views.
- **[Risk] Outline color change conflicts with existing transform-orange in `outline-drag-color`** → Mitigation: explicitly MODIFY that spec to light/off-white for translate session; keep API as “session/drag active” boolean.
- **[Risk] Session abstraction overbuilt for P1** → Mitigation: implement minimal surface (`beginFromHandle`, `onPointerMove`, `confirm`, `cancel`, feedback queries); no grab keymap in P1.
- **[Risk] Constraint guide / ghost draw cost** → Mitigation: reuse overlay line / gizmo draw paths; only while session active; rely on existing `requestViewportRedraw`.
- **[Trade-off] Product outline ≠ Blender** → Accepted in grilling; document in CONTEXT + specs.

## Migration Plan

- No asset or scene format migration.
- Behavior change is editor-only; ship behind normal editor build.
- Rollback: revert session wiring and restore previous `dragWorldPoint` path if needed.

## Open Questions

- Exact RGB for transform-active outline (pick a constant next to existing `#F57011` selection orange during implement; default proposal: near-white e.g. `(0.92, 0.92, 0.94)` unless art direction overrides).
- Whether view-aligned axis fallback threshold matches Blender’s epsilon — tune during manual QA.
