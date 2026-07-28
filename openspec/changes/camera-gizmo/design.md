## Context

Camera Component, Play resolve, Inspector FOV/near/far/Main, and authorship isolation already shipped (`player-gameplay-camera`). Edit Mode still has no Blender-like camera wire or Align actions. OverlaySystem already hosts grid, axes, Transform, Navigate, outline — Camera Gizmo joins that set behind `editorOverlaysEnabled`.

## Goals / Non-Goals

**Goals:**

- Blender-like Camera Gizmo draw + pick + FOV/clip handles (single selection).
- Align View to Camera / Align Camera to View with Play-consistent unselected target.
- History: FOV/clip release + Align Camera to View; Align View does not enter Document History.
- Player: no Camera Gizmo.

**Non-Goals:**

- Continuous viewport lock to Main Camera.
- Stored sensor aspect field (use editor viewport aspect + fixed display distance).
- Multi-select batch FOV/clip or Align under multi-select.
- Orthographic Camera Component gizmo variant (perspective FOV only this slice).
- Reworking Transform gizmo modes.

## Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| Visual language | Blender wire (origin, edges, view frame, up triangle) | User reference image |
| Aspect / depth | Viewport aspect + fixed local display distance | Grilled |
| Interaction | Full: pick, Transform pose, FOV/clip drag, Align both ways | Grilled |
| Unselected Align target | Main else first (EntityId order) | Match Play resolve |
| Hit order | Camera Gizmo before mesh pick | Match other overlays |
| History | Seal FOV/clip on release; Align Camera→View yes; Align View←Camera no | Document History rules |

## Risks / Trade-offs

- [FOV handle UX vs Transform gizmo] → Prefer frame-edge drag for FOV; Transform keeps pose; clear hover priority.
- [Line AA budget] → Prefer OverlayLinePass / existing line AA path over a one-off shader if possible.
- [Numpad conflicts] → Menu always works; laptop fallbacks required.

## Migration Plan

None. Existing scenes with Camera Components gain gizmos automatically.

## Open Questions

None — grilled and confirmed 2026-07-28.
