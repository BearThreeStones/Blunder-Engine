## Why

Blunder already supports auto-hide panels (edge strips, expand/collapse, pin via tab button), but **pinning requires clicking the pin affordance**—there is no ADS-style **drag-to-edge** path. Users expect to drag a dock tab or float window to the **outer host perimeter** to pin, with **split-dock guides** (center cross) remaining for the inner container area. The initial drag-to-pin implementation landed strip previews and detection, but guide visuals still use plain colored rectangles—not the Qt ADS pictographic cross and perimeter buttons users recognize. Closing the visual and interaction gap completes the auto-hide drag-drop milestone.

## What Changes

- Add **outer-edge auto-hide drop detection** during tab/float drag: outer hit band aligned with strip preview width (**32px** default, `max(preview, strip_thickness)` when strip exists), full edge span—not ADS 8px-only band. Perimeter guide hit rects span the full edge length (§7.17).
- Add **drag overlay visuals** with ADS-style differentiation:
  - **Auto-hide:** narrow strip preview on hovered edge **plus** four perimeter guide buttons (always visible during pinnable drags), each with directional icon and inward-pointing arrow
  - **Split:** five-direction cross guides restyled as ADS pictographic buttons (dark-theme adapted); preview at ~1/3 container size
- **Restyle** split cross guides as ADS **mini-window icons** (`DockGuideButton`): blue frame, top chrome bar, half-pane active fill, **dashed** split divider (segment rects; Slint has no native dash stroke), dark-theme adapted
- **Add** perimeter auto-hide guides as **`DockAutoHideGuideButton`** (§7.9–7.14): edge-oriented mini-window icons — **landscape 48×28** on top/bottom, **portrait 28×48** on left/right; full light-blue body, white inward arrow inside Slint; highlight = bold blue frame only
- Wire **drop on auto-hide edge** to `setWidgetAutoHide` (and float-window equivalent), gated on `DockWidgetFeature::pinnable`.
- Support **tab insert index** when dropping onto an edge strip that already has auto-hide tabs (ADS `tabIndexUnderCursor`).
- Extend `endDrag` / float drop paths so OS float drags can pin to an edge (ADS `dropIntoAutoHideSideBar`).
- **Modified behavior:** inner-edge left/right/top/bottom slots continue to mean split-dock; outer-edge means auto-hide only when pinnable. Split cross guides and auto-hide perimeter guides MAY be visible simultaneously (remove mutual exclusion in `editor_window.slint`).
- **Fix (§7.20):** In-host tab drags SHALL use **global pointer poll** after drag threshold so `handleDragMove` keeps receiving dock-local coordinates when the cursor leaves the tab `TouchArea` (root cause of “only corners/edge centers work” despite widened hit bands).

## Capabilities

### New Capabilities

- `dock-auto-hide-drag-drop`: Drag detection, overlay, drop execution, and ADS-style guide visuals for pinning widgets to host edge strips during dock tab drag (including float-origin drops and tab insert ordering).

### Modified Capabilities

- `docking-auto-hide`: Add requirements for drag-to-pin gesture, outer-edge drop zones, perimeter guide buttons with arrows, and visual overlay behavior during drag (complements existing pin-button and strip lifecycle requirements).

## Impact

- **C++:** `dock_manager.cpp` (`appendGuides`, `appendAutoHideGuides`, `handleDragMove`, `endDrag`, hit-test expansion), `dock_drag_controller.*`, `dock_auto_hide.*`, `dock_layout_model.h`
- **Slint:** `docking_panel.slint` (`DockGuideButton` for split cross only; new `DockAutoHideGuideButton` with embedded arrow; `DockAutoHideGuidesLayer`, strip preview layer), `editor_window.slint` (layer ordering, remove guide mutual exclusion)
- **C++ UI bridge:** `slint_system.cpp` (`syncDockingWorkspace`); simplify `DockAutoHideGuide` model (drop separate `arrow_rect` fields; single guide rect includes arrow + icon)
- **C++ layout:** `dock_auto_hide.cpp` — edge-oriented guide dimensions (landscape vs portrait icon); hit-test uses combined Slint bounds
- **Specs:** Delta on `docking-auto-hide` and `dock-auto-hide-drag-drop`
- **Non-goals (this change):** `open_on_drag_hover` auto-expand on drag hover (optional follow-up); batch pin of every widget in a multi-area float container beyond pinnable dragged widget
