## Context

Tab interaction in `docking_panel.slint` fires `tab-activated` and `tab-pressed` (→ `beginDrag`) on the same pointer down. On release, `dock_slint_bridge` calls `handleDragMove` then `endDrag`. `handleDragMove` hit-tests the full container rect (tab bar + content) and `computeSlotRaw` maps tab-bar Y to `DockSlot::top`. With two widgets in one container, `dockWidget(..., top, ...)` passes the `widgetCount == 1` guard and commits a vertical split.

`DockDragController` already tracks `m_drag_start_pointer`, `m_pointer`, and `m_threshold_px` (default 8px) for **preview** activation, but `endDrag` does not use that threshold for drop/float decisions.

## Goals / Non-Goals

**Goals:**

- Clicking a tab in a multi-tab container switches the active widget only; dock tree unchanged on no-move release.
- Tab drag past threshold retains preview, guides, split dock, float, and auto-hide drop behavior.
- Edge-slot detection during drag hover uses content area geometry so the chrome row is not a split edge.

**Non-Goals:**

- Changing center-merge or guide visuals.
- Reworking native-float global pointer poll.
- Slint-side deferred `tab-pressed` unless C++ gating proves insufficient.

## Decisions

### 1. Gate `endDrag` commits on drag distance (primary fix)

**Choice:** In `DockManager::endDrag`, if `DockDragController` reports pointer movement below `m_threshold_px` from drag start, reset drag without `dockWidget`, `makeFloatingFor`, or auto-hide drop. `activateWidget` already ran on pointer down.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Defer `beginDrag` until Slint detects movement | Touches Slint + bridge; global poll timing harder |
| Same-container edge no-op always | Blocks intentional “split tab out” from same container |
| Only fix in Slint (separate click handler) | Duplicates threshold logic; release path still risky |

**Rationale:** Reuses existing 8px threshold from `dock-floating-drag-preview`; minimal diff; matches user mental model (click vs drag).

### 2. Use content rect for slot computation during drag hover (secondary fix)

**Choice:** When `handleDragMove` hit-tests a container, pass a **content rect** (full container rect minus `chrome_bar_height` from the top) into `computeSlotRaw` / `stabilizeSlot` / `previewRectForSlot`. If pointer is only over the tab bar band, treat as `DockSlot::center` or skip hover (no guide highlight on chrome-only hover).

**Rationale:** Defense in depth if user moves slightly past threshold while still over tabs; aligns resize hit-testing which already excludes chrome row.

### 3. Add `DockDragController::exceededDragThreshold()` helper

**Choice:** Expose distance check used by `endDrag` (and optionally document for tests). Keep threshold sourced from `beginDrag` argument (`m_metrics.tab_bar_height` today — verify alignment with 8px default or pass explicit `k_default_drag_threshold`).

**Note:** `beginDrag` currently passes `m_metrics.tab_bar_height` as threshold — may be ~32px not 8px. Design: **standardize on `DockDragController::k_default_drag_threshold` (8px)** for click-vs-drag gating in `endDrag`, independent of preview title-bar bias. Preview activation can remain as-is.

## Risks / Trade-offs

- **[Risk] Very small intentional drags (<8px) no longer float/split** → Acceptable; matches preview spec; user can move slightly more.
- **[Risk] Content-rect slot change alters edge sensitivity near tab/content boundary** → Mitigate with hysteresis already in `stabilizeSlot`; manual test top-edge split from content.
- **[Risk] `tab-activated` on wrong tab if user meant to drag** → Existing behavior; activation on press is standard for IDE tabs.

## Migration Plan

Single PR; no data migration. Manual validation on multi-tab container after center merge. Rollback: revert `endDrag` gate and content-rect slot change.

## Open Questions

- Should `beginDrag` threshold argument be unified to 8px for preview and endDrag? **Recommend yes during apply.**
- Float window `tab-activated` is wired no-op in `floating_panel_window.slint` — out of scope unless same bug reported there.
