## Context

Transform gizmo rendering already supports per-handle `highlight` → `gizmoAxisColor().color_hi` in `TransformGizmoOverlay::recordGizmoDraw`, but today `highlight` is only true when `isDragging() && getActiveAxis() == axis`. Click/drag picking uses CPU analytic tests in `transform_gizmo_pick.cpp` (`pickTranslationGizmoHandle`, etc.) from `TransformGizmoController::onMousePressed`.

`gizmo-rotate-trackball-scale-style` explicitly deferred `WM_GIZMO_DRAW_HOVER`. Hybrid GPU mesh picking runs after gizmo in `RenderSystem::onEvent` and must not be blocked by hover updates.

## Goals / Non-Goals

**Goals:**

- Highlight the interactive handle under the cursor in translate/rotate/scale modes using existing `color_hi` visuals.
- Reuse the same pick functions and thresholds as click/drag (no GPU pick for gizmo).
- Redraw only when hovered axis changes; drag highlight takes precedence.
- Unit-test highlight precedence and `pickTransformGizmoHandle` wrapper.

**Non-Goals:**

- Cursor shape changes.
- Hover on non-pickable decor: trackball (`rot_t`), outer ring (`rot_c`), scale annulus (`scale_c_outer`).
- Navigate gizmo hover.
- New shaders or color themes beyond existing `color` / `color_hi`.

## Decisions

### 1. CPU pick on MouseMoved (not GPU)

**Decision:** Call existing analytic pick on `MouseMoved` when gizmo mode active, not dragging, pointer in viewport.

**Rationale:** Zero-frame, same geometry as click; matches ASTRYN/hybrid-pick gizmo priority model. Cost is O(handles) per move — acceptable (~7 handles).

**Alternative:** GPU ID buffer for gizmo — rejected; unnecessary complexity.

### 2. Hover state on TransformGizmoController

**Decision:** Add `std::optional<ManipulatorAxis> m_hover_axis`; expose `updateHoverFromPointer`, `clearHover`, `isHandleHighlighted`.

**Rationale:** Controller already owns drag state and pick context; overlay reads controller for draw.

### 3. Highlight precedence helper

**Decision:** `gizmoHandleHighlighted(active, hover, axis)` — if `active` set, only active highlights; else hover.

**Rationale:** Single source for overlay and tests; drag suppresses hover automatically when `m_active_axis` set.

### 4. Input wiring without event.handled

**Decision:** `RenderSystem::onEvent` calls `updateHoverFromPointer` on `MouseMoved`; never sets `event.handled` for hover.

**Rationale:** `ViewportPickSystem::onViewportPointerMoved` and camera must still receive moves. Gizmo drag path unchanged (controller sets `handled` on press/move during drag).

### 5. Redraw on hover change only

**Decision:** `updateHoverFromPointer` returns `bool`; caller calls `requestViewportRedraw()` when true.

**Rationale:** Avoid full viewport redraw every mouse pixel when hover unchanged.

### 6. Extract buildPickContext from onMousePressed

**Decision:** Share scale/pick context construction between press and hover.

**Rationale:** DRY; prevents hover/click threshold drift.

## Risks / Trade-offs

- **[Risk] Extra pick work every mouse move in viewport** → Only when gizmo mode active and not dragging; cheap analytic tests.
- **[Risk] Hover stuck after leaving viewport** → Clear hover when pointer outside viewport or mode/selection invalid.
- **[Risk] Flicker between overlapping handles** → Existing `pickBest` distance tie-break same as click; acceptable.

## Migration Plan

Single PR; no data migration. Manual QA: G/R/S modes, drag precedence, mesh pick regression on handle miss.

## Open Questions

- None — use existing `color_hi` alpha (1.0) for hover, matching drag highlight.
