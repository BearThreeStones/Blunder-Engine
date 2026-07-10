## 1. Pointer routing prerequisite

- [x] 1.1 Verify or land in-host drag pointer routing (`dragNeedsGlobalPointerPoll` + `tickGlobalDockPointerPoll`, or `DockDragCaptureLayer`); manual test: left host edge mid-height receives updates during drag
- [x] 1.2 Gate `tab-moved` / `tab-released` when global poll or capture layer is active

## 2. Metrics and layout model

- [x] 2.1 Add `dock_outer_margin`, `host_guide_proximity`, `host_dock_preview_fraction` (0.5) to `DockLayoutMetrics`
- [x] 2.2 Add `DockHostGuideView`, `host_guides`, `host_dock_preview` to `DockLayoutModel`
- [x] 2.3 Add `previewRectForHostSlot(host_rect, slot)` in `dock_manager.cpp`
- [x] 2.4 Add `computeHostGuideLayouts` (or reuse/adapt guide layout helper) for centered edge icon rects

## 3. Drag controller state

- [x] 3.1 Extend `DockDragController` with `hovering_host_guide`, `hovered_host_slot`, set/clear helpers
- [x] 3.2 Add `markHostDockDropped()` branch; ensure mutual exclusion between host / auto-hide / cross hover
- [x] 3.3 Change cross hover to icon-rect hit only (remove `computeSlotRaw` path from `handleDragMove` for drag)

## 4. Three-layer hit testing

- [x] 4.1 Implement hit priority in `handleDragMove`: host icon → auto-hide edge zone → cross icon (inner region)
- [x] 4.2 Remove `hitAutoHideGuideEdge` from drag path; auto-hide uses `computeAutoHideDropEdge` only
- [x] 4.3 Add `isInsideOuterMargin(host, pointer, margin)` to suppress cross when near host perimeter
- [x] 4.4 Implement host guide icon hit test (`hitHostGuideSlot`)
- [x] 4.5 Implement cross guide icon hit test (`hitCrossGuideSlot` on container rect)
- [x] 4.6 Wire `endDrag`: host → `dockToRoot`; auto-hide → `setWidgetAutoHide`; cross → `dockWidget`

## 5. buildLayoutModel and sync

- [x] 5.1 Emit host guides with faint/highlight states from proximity + icon hover
- [x] 5.2 Emit half-window `host_dock_preview` when host guide highlighted
- [x] 5.3 Emit cross guides faint when container eligible; highlight icon under cursor only
- [x] 5.4 Stop emitting `auto_hide_guides` / `DockAutoHideGuidesLayer` during drag
- [x] 5.5 Sync new fields in `slint_system.cpp`

## 6. Slint layers

- [x] 6.1 Add `DockHostGuidesLayer` with `DockGuideButton` icons + half-window preview rect
- [x] 6.2 Add faint/highlight opacity to `DockGuideButton` (host + cross)
- [x] 6.3 Update `DockGuidesLayer`: faint all guides when `drag-active`; highlight per `guide.highlighted`
- [x] 6.4 Wire `DockHostGuidesLayer` in `editor_window.slint`; remove or hide `DockAutoHideGuidesLayer` during tab drag
- [x] 6.5 Optional: `DockDragCaptureLayer` atop host if poll insufficient (skipped — using global pointer poll)

## 7. Manual validation

- [ ] 7.1 Drag tab near right edge: four host guides faint; hover R icon → half-window blue preview; release → host-level right dock
- [ ] 7.2 Drag pinnable tab to left edge mid-height (not on host icon): narrow strip preview; release → auto-hide pin
- [ ] 7.3 Drag over viewport container interior: five faint cross icons; hover left icon → 1/3 preview; release → split dock
- [ ] 7.4 Pointer in outer margin over container: no cross guides; edge zone may still auto-hide
- [ ] 7.5 Host icon hover on left edge suppresses auto-hide for that pointer position
- [ ] 7.6 Non-pinnable viewport drag: no auto-hide strip; host guides and cross still behave per gates
- [ ] 7.7 Left host edge full height: auto-hide strip activates without requiring top-left corner only
