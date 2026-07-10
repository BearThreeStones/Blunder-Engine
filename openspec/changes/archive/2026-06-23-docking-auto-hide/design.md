## Context

Blunder implements a **custom** docking stack (not Qt ADS):

| Layer | Location | Role |
|-------|----------|------|
| Model | `DockManager`, `DockNode`, `DockWidget` | Split tree, tabs, float, drag |
| Layout output | `DockLayoutModel` | Rects for Slint each frame |
| Presentation | `editor_window.slint`, `docking_panel.slint` | Tiles, tabs, splitters, guides |
| Integration | `SlintSystem::syncDockingWorkspace()` | Pushes model → Slint; caches viewport/hierarchy/browser rects |

Auto-hide does **not** exist today. `collapseEmptyContainer()` only removes empty tab containers from the split tree after close/drag — it is unrelated.

**Reference model (ADS 4.0):** `CAutoHideSideBar` (per edge) → `CAutoHideTab` (collapsed label) → `CAutoHideDockContainer` (expanded overlay hosting `CDockAreaWidget` + `CResizeHandle`). We mirror this semantics in engine-native types without Qt.

**Constraints:**

- Slint owns window composition; overlays must be explicit high-`z` layers in `editor_window.slint`.
- Viewport logical rect comes from `syncDockingWorkspace()` scanning `DockPanelKind::viewport` tiles — auto-hide must update this when panels pin/unpin or expand/collapse.
- `isDockLayoutDragActive()` / `isSplitterResizeInteractionActive()` currently return `false` in `slint_system.h`; auto-hide drag/hover must participate in defer logic.

## Goals / Non-Goals

**Goals:**

- Pin any **pinnable** `DockWidget` to left/right/top/bottom; remove it from the main split layout.
- Collapsed: show only an edge **tab strip** (one tab per auto-hidden widget on that edge).
- Expanded: show widget content in an **overlay** above the dock host, with inner-edge **resize handle** (min 64 px, max derived from host minus strip).
- Support click tab → toggle expand/collapse; optional hover expand/collapse with delay; optional click-outside collapse; close button collapses (config) vs detaches.
- Unpin restores widget to previous dock location (or sensible default slot).
- `DockAutoHideConfig` bitmask (ADS-like flags) with `DefaultAutoHideConfig` at `DockManager` construction.
- Serialize auto-hide state in a later task only if layout persistence already exists — **v1 may be session-only**.

**Non-Goals:**

- Porting Qt ADS source or adding Qt dependency.
- Auto-hide for **floating** `DockNode` windows (float remains separate).
- Scrollable multi-page sidebars (v1: vertical stack on left/right, horizontal on top/bottom; clip if overflow).
- Layout save/load to disk (unless trivial hook exists).
- Per-area "auto-hide all widgets in container" menu (v1: per-widget pin; area-wide toggle deferred).
- Animations beyond instant show/hide (Slint opacity optional stretch).

## Decisions

### 1. New types alongside `DockNode`, not a fourth `DockNodeKind`

**Decision:** Add `DockAutoHideEntry` (one widget pinned to one edge) owned by `DockManager` in four edge lists (`m_auto_hide_left`, etc.). Auto-hidden widgets are **removed** from their `DockNode` container and stored in the entry with a **restore snapshot** (`DockRestoreHint`: parent node id, slot, tab index).

**Rationale:** ADS keeps auto-hide containers outside the central dock area. Folding into `DockNodeKind::auto_hide` would complicate `solveNode()` split math.

**Alternative:** New node kind in tree — rejected; split solver would need strip insets on every layout pass.

### 2. Layout pipeline: insets + overlay pass

**Decision:** `buildLayoutModel()` runs in two phases:

1. **Compute strip insets** from auto-hide tab counts and `DockLayoutMetrics::auto_hide_strip_thickness` (default ~28 px).
2. **Solve main tree** in `host_rect` shrunk by insets (left/right/top/bottom).
3. **Emit auto-hide views**: `DockAutoHideTabView` per collapsed tab; `DockAutoHideOverlayView` per **expanded** entry (content rect + resize handle rect + title bar if enabled).

Expanded overlay does **not** participate in split tree; it uses absolute coords within the dock host, sized from cached `expanded_size` on the entry.

```
┌─strip─┬──────────────────────────────────────┬─strip─┐
│ [H]   │                                      │ [I]   │
│ [C]   │         Main split tree              │       │
│       │         (viewport, etc.)             │       │
├───────┴──────────────────────────────────────┴───────┤
│              [Content Browser tab]                    │
└──────────────────────────────────────────────────────┘

Expanded Inspector (overlay, z above main):
        ┌──────────────────┐
        │ Inspector    [_] │
        ├──────────────────┤
        │   panel content  │◄── resize handle on inner edge
        └──────────────────┘
```

### 3. ADS class mapping

| ADS | Blunder |
|-----|---------|
| `CAutoHideSideBar` | Edge list on `DockManager` + `DockAutoHideTabView[]` in layout model |
| `CAutoHideTab` | `DockAutoHideTabView` + Slint `DockAutoHideTab` component |
| `CAutoHideDockContainer` | `DockAutoHideEntry` state + `DockAutoHideOverlayView` when `expanded` |
| `CResizeHandle` | `DockAutoHideResizeHandleView` + splitter-like drag in manager |
| `CDockWidget::setAutoHide` | `DockManager::setWidgetAutoHide(widget_id, edge, enable)` |
| `unpinDockWidget` | `DockManager::unpinAutoHide(widget_id)` |
| `collapseView` / `toggleCollapseState` | `collapseAutoHide(widget_id)` / `toggleAutoHideExpanded(widget_id)` |
| `handleAutoHideWidgetEvent` | `SlintSystem` pointer enter/leave + deferred timer (~300 ms default) |
| `eAutoHideFlag` | `enum class DockAutoHideFlag : uint32_t` on `DockManager` |

### 4. Pinnable widgets

**Decision:** `DockWidget::features()` returns `DockWidgetFeature::pinnable` by default; `viewport` is **not** pinnable in v1.

**Rationale:** Hiding the only viewport would break editor; ADS uses `DockWidgetPinnable` similarly.

### 5. Slint structure

**Decision:** Extend `docking_panel.slint`:

- `DockAutoHideTabsLayer` — renders tab strips on four edges from `docking-auto-hide-tabs` model.
- `DockAutoHideOverlaysLayer` — renders expanded frames + content placeholders (panel kind dispatches like existing tiles).
- Tab callbacks: `auto-hide-tab-pressed`, `auto-hide-tab-hovered`, `auto-hide-tab-unpin`.
- Overlay callbacks: `auto-hide-overlay-resize-moved`, `auto-hide-collapse-requested`.

In `editor_window.slint`, place overlays **above** main tiles but **below** `ViewportProjectionToggle` (z 2000) and `ImportMeshDialog` (z 3000). Use z ~1500 for auto-hide overlays.

### 6. Configuration flags

**Decision:** Mirror ADS defaults subset:

```cpp
enum class DockAutoHideFlag : uint32_t {
  feature_enabled           = 0x01,
  dock_area_has_pin_button  = 0x02,  // v1: tab context or titlebar pin affordance
  show_on_mouse_over        = 0x20,
  close_button_collapses    = 0x40,
  has_close_button          = 0x80,
  has_minimize_button       = 0x100,
  open_on_drag_hover        = 0x200,
  close_on_outside_click    = 0x400,
  default_config = feature_enabled | dock_area_has_pin_button
                 | has_minimize_button | close_on_outside_click,
};
```

Set on `DockManager` at init (before `seedDockingWorkspace`); no global singleton required.

### 7. Interaction and defer wiring

**Decision:**

- `isDockLayoutDragActive()` → `m_dock_manager.drag().isActive()`.
- `isSplitterResizeInteractionActive()` → track `m_auto_hide_resize_active` OR existing splitter drag flag on manager.
- Hover timer lives in `SlintSystem` (like ADS `DelayedAutoHideTimer`): on tab enter schedule expand; on leave schedule collapse if expanded and `show_on_mouse_over`.

Click-outside: on SDL mouse up, if expanded and flag set, hit-test overlay rects; if miss, `collapseAllAutoHide()`.

### 8. Close button semantics

**Decision:** When `close_button_collapses` and widget is auto-hidden + expanded, tab/overlay close triggers `collapseAutoHide` not `closeWidget`. Otherwise existing `closeWidget` → unpin + detach behavior.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Viewport rect stale after pin/unpin | `m_docking_model_dirty = true`; `syncDockingWorkspace()` already rebuilds viewport origins |
| Overlay blocks viewport input | Collapse on outside click; overlay only captures events inside its frame |
| Hover timer fights click toggle | Stop timer on mouse down (ADS pattern) |
| Many tabs on one edge overflow | Clip strip; defer scrolling to v2 |
| Restore hint invalid after tree edits | On unpin, if parent missing, `dockToRoot(widget, opposite_edge_slot)` |
| Performance: full Skia refresh each expand | Reuse `markFullSkiaRefresh()` only on expand/collapse; strip changes are cheap |

## Migration Plan

1. Ship behind `DockAutoHideFlag::feature_enabled` (default **on** with ADS-like default config).
2. No change to `seedDockingWorkspace()` until user pins a panel.
3. Rollback: disable feature flag → ignore auto-hide entries in layout build (or clear entries on shutdown).

## Open Questions

- **Pin affordance in v1:** Tab right-click menu vs dedicated pin icon on tab bar? → Start with **pin** action on tab context (Slint) or keyboard/debug command; titlebar pin button in v1.1.
- **Session persistence:** Defer unless `DockManager` already has layout serialization hook.
- **Console + Content Browser tab group:** Pinning one tab in a shared container — pin active widget only (ADS behavior).
