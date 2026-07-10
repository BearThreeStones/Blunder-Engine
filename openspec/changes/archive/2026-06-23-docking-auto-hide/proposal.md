## Why

Blunder's editor docking system supports splits, tabs, drag-and-drop, and floating panels, but every docked panel permanently consumes layout space. DCC tools and IDEs (Qt ADS 4.0, Visual Studio, Unreal) offer **Auto-Hide**: panels pin to an edge as a slim tab strip and expand as an overlay when needed. This frees viewport and central workspace area without removing panels from the session.

## What Changes

- Add **Auto-Hide** mode for dock widgets: pin to left/right/top/bottom edge, collapse to a **side tab strip**, expand as a **floating overlay** above the main dock content.
- Introduce C++ layout types and `DockManager` APIs modeled on ADS 4.0 (`CAutoHideSideBar`, `CAutoHideTab`, `CAutoHideDockContainer`), adapted to Blunder's `DockNode` / `DockLayoutModel` / Slint bridge.
- Extend `DockLayoutModel` with auto-hide sidebars, edge tabs, and expanded overlay rects (including inner-edge resize handles).
- Add Slint layers for edge tab strips and expanded overlays; wire callbacks through `SlintSystem` (alongside existing docking callbacks).
- Implement **pin / unpin**, **collapse / expand**, optional **hover-to-expand** with delay timer, **click-outside-to-collapse**, and **close-button-collapses** behaviors behind a `DockAutoHideConfig` flag set.
- Exclude auto-hidden widgets from the main split tree layout so the viewport and remaining panels reclaim space; side strips consume only configured strip width.
- Wire `isDockLayoutDragActive()` / `isSplitterResizeInteractionActive()` to real dock state (currently stubbed `false`) so auto-hide interaction defers correctly with existing viewport pacing.
- **Non-breaking**: default layout unchanged until the user pins a panel; `seedDockingWorkspace()` behavior preserved.

## Capabilities

### New Capabilities

- `docking-auto-hide`: Pin dock widgets to container edges, collapsed tab strips, overlay expand/collapse, resize handles, configuration flags, and Slint presentation.

### Modified Capabilities

- _(none — no existing OpenSpec specs for editor docking)_

## Impact

- **Primary**: `engine/src/runtime/function/ui/docking/` (`dock_types.h`, `dock_widget.h`, `dock_layout_model.h`, `dock_manager.h/.cpp`, new `dock_auto_hide.*`).
- **Slint UI**: `engine/src/runtime/function/slint/docking_panel.slint`, `editor_window.slint`; `slint_system.cpp` (`syncDockingWorkspace`, layout rect caching, input routing).
- **Integration**: `dock_slint_bridge.*` (or inline in `SlintSystem`) for auto-hide model vectors and callbacks.
- **Docs**: optional note in `docs/agents/structure.md` or a short docking section when behavior is user-visible.
- **Risk**: overlay z-order vs viewport projection toggle / import dialog; hit-testing for click-outside; layout invalidation during expand/collapse affecting viewport off-screen size.
