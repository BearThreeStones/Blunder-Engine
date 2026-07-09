## Context

P1 (`translate-modal-handle-parity`) introduced `TranslateModalSession` with handle entry, screen-space motion, drag feedback, cursor, and transform-active outline. Grab (`G`) was explicitly deferred so handle UX could land first.

Blender starts free translate via `TRANSFORM_OT_translate` from the `G` keymap. Confirm is typically a second LMB click (not button-release), with RMB / Escape cancel. P1 handle path uses LMB **release** confirm; P2 grab must use **click-to-confirm** without forking motion/feedback owners.

Domain terms: `CONTEXT.md` → “Transform gizmo (translate)”. Add a **Grab entry** term during implement if missing.

## Goals / Non-Goals

**Goals:**

- `G` with a selection begins a Translate Modal Session in free (view-plane) constraint via `beginFromGrab`.
- Reuse P1 motion, cursor, outline, inspector sync, redraw, and cancel restore.
- Grab confirm = LMB **click** (press); cancel = RMB / Escape with TRS restore.
- Grab feedback: no constraint guides; no active-handle ghost; hide solid free-move center; keep three reference axis arrows on the live pivot; planes follow the same “center/free” visibility as P1 free move (planes remain visible).

**Non-Goals:**

- Keyboard constraints, MMB axis pick, numeric input (P3).
- `R` / `S` grab modals.
- Changing handle confirm semantics (handles stay release-to-confirm).
- Requiring translate gizmo mode to be active before `G` (Blender-like: `G` starts translate grab whenever a selection exists).

## Decisions

### 1. Entry API: `beginFromGrab` on the same session type

**Decision:** Extend `TranslateModalSession` with `beginFromGrab(pointer, object_position, camera)` that sets free constraint (`trans_c` / unconstrained) and an entry kind `grab`. Controller routes `G` → begin + snapshot full TRS.

**Alternatives:** Separate `GrabModalSession`. Rejected — P1 architecture exists to avoid path divergence before P3.

### 2. Confirm semantics keyed by entry kind

**Decision:**

| Entry | Confirm | Cancel |
|-------|---------|--------|
| Handle | LMB **release** (P1) | RMB / Escape |
| Grab | LMB **press/click** | RMB / Escape |

While grab-active, the first LMB press in the viewport confirms (does not start a handle pick). Camera orbit remains locked for the session (same as handle drag lock).

**Alternatives:** Unify both to click-to-confirm. Rejected — would regress P1 handle UX locked in grilling.

### 3. `G` availability

**Decision:** With a valid selection and no active translate modal session, `G` begins grab. Does **not** require `TransformGizmoMode::translate`. If another gizmo drag is active, ignore `G`. No selection → ignore.

**Alternatives:** Only when translate mode is selected. Rejected — weaker Blender parity for keyboard-first workflow.

### 4. Grab feedback vs handle feedback

**Decision:**

- Motion: same free view-plane path as center handle.
- Guides: none.
- Ghost: none (no pressed handle).
- Origin dot: none (free session).
- Cursor + transform-active outline: same as any active translate modal session.
- Visibility: hide solid center disc; keep reference axis arrows on live pivot; keep plane handles visible (match P1 center-drag visibility).

### 5. Key routing ownership

**Decision:** Handle `G` in `TransformGizmoController::onKeyPressed` (or the existing editor key path that already owns Escape for gizmo cancel), so session ownership stays with the gizmo controller. Do not invent a second modal stack in Slint.

## Risks / Trade-offs

- **[Risk] LMB click confirm races with viewport pick** → Mitigation: while grab session active, consume LMB press for confirm before pick/gizmo hit tests.
- **[Risk] `G` conflicts with future text fields / shortcuts** → Mitigation: only when viewport/editor 3D focus owns keys and no text input focus (follow existing Escape/gizmo key gating).
- **[Risk] Entry-kind bugs flip confirm semantics** → Mitigation: unit-test confirm policy helper; manual QA both handle release and grab click.
- **[Trade-off] Planes stay visible during grab** → Matches P1 free-center visibility; P3 may hide more when constraints engage.

## Migration Plan

- Editor-only behavior; no asset migration.
- Requires P1 session + outline/cursor wiring present on the branch.
- Rollback: remove `G` routing and `beginFromGrab`; handle path unchanged.

## Open Questions

- Whether holding LMB after grab confirm should be a no-op only (yes — session already ended).
- Exact key when Slint text focus is active — reuse whatever Escape already uses for “editor 3D shortcuts enabled.”
