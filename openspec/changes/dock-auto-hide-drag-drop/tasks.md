## 1. Metrics and layout model

- [x] 1.1 Add `auto_hide_drop_mouse_zone` (8px) and `auto_hide_drop_preview_size` (32px) to `DockLayoutMetrics`; document ADS alignment in comments
- [x] 1.2 Add `DockAutoHideDropPreviewView` to `DockLayoutModel` (`visible`, `edge`, `rect`)
- [x] 1.3 Add `computeAutoHideDropEdge(host, pointer, strip_thickness_per_edge)` and `computeAutoHideDropPreviewRect(edge, host, ...)` in `dock_auto_hide.cpp`

## 2. Drag controller state

- [x] 2.1 Extend `DockDragController` with `hoveredAutoHideEdge`, `autoHideTabInsertIndex`, clear/set helpers
- [x] 2.2 Implement `hitAutoHideTabInsertIndex(pointer, edge)` using current auto-hide tab rects on that edge
- [x] 2.3 Gate auto-hide hover on `feature_enabled` + dragged widget `pinnable`

## 3. Hit testing and drag move

- [x] 3.1 In `handleDragMove`, resolve outer auto-hide edge before inner `computeSlotRaw` split slot
- [x] 3.2 When auto-hide edge active, suppress split guide hover; when in inner container band, use existing split logic
- [x] 3.3 Widen mouse zone per edge when that edge already has visible strip tabs (`max(8px, strip_thickness)`)

## 4. Drop execution

- [x] 4.1 Extend `setWidgetAutoHide` with optional `insert_index`; reorder `m_auto_hide_entries` on same edge
- [x] 4.2 In `endDrag`, branch to `setWidgetAutoHide` when auto-hide edge hovered (docked tab origin)
- [x] 4.3 In `endDrag`, handle native float tab origin: pin widget + tear down/reconcile float window
- [x] 4.4 Adjust `previewRectForSlot` to ~1/3 container factor for split previews (ADS alignment)

## 5. Slint overlay visuals (strip preview — landed)

- [x] 5.1 Add `DockAutoHideDropPreview` struct + `DockAutoHideDropPreviewLayer` in `docking_panel.slint` (narrow strip preview)
- [x] 5.2 Wire layer in `editor_window.slint` above dock host, below or beside existing `DockGuidesLayer`
- [x] 5.3 Sync `auto_hide_drop_preview` from `buildLayoutModel` in `slint_system.cpp`
- [x] 5.4 Ensure split guides remain hidden while auto-hide strip preview is active *(superseded by §7 — guides coexist)*

## 6. Manual validation (drag-drop behavior)

- [ ] 6.1 Drag pinnable panel to outer left **32px** band (any height along edge): strip preview; release pins to left strip
- [ ] 6.2 Drag same panel to inner left split zone: cross guides + ~1/3 width preview; release split-docks
- [ ] 6.3 Drag viewport to outer edge: no auto-hide preview; no pin on release
- [ ] 6.4 Drop onto edge with existing tabs: new tab inserts at cursor position along strip
- [ ] 6.5 Drag tab from OS float to outer edge: pins widget and closes float chrome
- [ ] 6.6 Pin button on tab still pins to default edge (regression)

## 7. ADS-style guide visuals (dark theme)

- [x] 7.1 Add `DockGuideButton` Slint component: dark grey button, cyan direction bar per slot, center tab-merge glyph, highlighted state
- [x] 7.2 Restyle `DockGuidesLayer` to use `DockGuideButton` instead of flat rectangles; keep existing C++ guide positions
- [x] 7.3 Add `DockAutoHideGuide` struct, `DockAutoHideGuideView` in C++, and `appendAutoHideGuides(host_rect, active_edge, model)`
- [x] 7.4 Add `DockAutoHideGuidesLayer` with perimeter buttons + inward arrows (`auto_hide_guide_arrow_size`, `auto_hide_guide_arrow_gap` metrics)
- [x] 7.5 Emit auto-hide guides for entire pinnable drag in `buildLayoutModel`; sync in `slint_system.cpp`
- [x] 7.6 Wire `DockAutoHideGuidesLayer` in `editor_window.slint`; remove mutual exclusion hiding split guides during auto-hide preview
- [x] 7.7 Expand auto-hide hit-test in `handleDragMove` to perimeter guide button rects (guide hit > outer band > split)
- [x] 7.8 Restyle `DockGuideButton` as ADS mini-window icon: blue frame, chrome bar, half-pane active fill, **dashed** split divider (`DockGuideDashedHLine` / `DockGuideDashedVLine` segment rects; Slint has no native dash stroke)
- [x] 7.9 Add `DockAutoHideGuideButton`: flat-wide (48×28), full light-blue body, chrome bar, **no** dashed divider; white inward `Path` triangle on outer side; highlight = bold blue frame only
- [x] 7.10 Switch `DockAutoHideGuidesLayer` from `DockGuideButton` + `DockGuideInwardArrow` to `DockAutoHideGuideButton`; remove `DockGuideInwardArrow` if unused
- [x] 7.11 Add `auto_hide_guide_width` / `auto_hide_guide_height` to `DockLayoutMetrics`; update `computeAutoHideGuideLayouts` for flat-wide combined bounds
- [x] 7.12 Simplify `DockAutoHideGuide` / `DockAutoHideGuideView`: drop `arrow_rect`, `show-arrow`, and Slint arrow-* fields; hit-test uses single guide `rect`
- [x] 7.13 Update `slint_system.cpp` sync for simplified auto-hide guide model
- [x] 7.14 Left/right perimeter guides use **portrait** icon (28×48); top/bottom keep **landscape** (48×28); update `computeAutoHideGuideLayouts` combined bounds (39×48 vs 48×39)
- [x] 7.15 `DockAutoHideGuideButton` / `DockAutoHideGuideMetrics`: derive icon width/height from `edge` (portrait vs landscape)
- [x] 7.16 Visibility threshold on left/right uses portrait icon height (48px) for `host.height >= 2×` check
- [x] 7.17 Widen outer auto-hide hit: `computeAutoHideDropEdge` uses `max(preview_size, strip_thickness)` with `preview_size=32` default (replace 8px-effective band)
- [x] 7.18 Expand `hitAutoHideGuideEdge` to **full-edge** hit bands (full host height/width × guide depth); keep Slint visual guide rects centered
- [x] 7.19 Pass `auto_hide_drop_preview_size` into drop-edge detection from `handleDragMove`; document metrics in `dock_manager.h`

- [x] 7.20 Extend `dragNeedsGlobalPointerPoll()` to return true for in-host drags after `exceededDragThreshold()` (native float unchanged: always poll)
- [x] 7.21 In `tickGlobalDockPointerPoll`, select `pointerToDockLocal` for in-host poll and `screenToDockLocal` for native-float poll (not gated on `dragNeedsGlobalPointerPoll` for coord path)
- [x] 7.22 Gate `on_docking_tab_moved` / `on_docking_tab_released` to no-op when `dragNeedsGlobalPointerPoll()` is true
- [ ] 7.23 Manual: drag Hierarchy tab along full left edge (any y) — auto-hide preview highlights for entire edge, not only near tab origin

## 8. Manual validation (guide visuals)

- [ ] 8.1 Pinnable drag: four perimeter guides with embedded white arrows visible for whole drag
- [ ] 8.2 Hover perimeter left guide: **bold blue frame** highlight + strip preview; body/arrow unchanged; split cross still visible over container
- [ ] 8.3 Split cross guides unchanged: mini-window with half-pane fill and **dashed** divider; center = tab merge without divider
- [ ] 8.4 Top/bottom guides are **wider than tall** (48×28); left/right guides are **taller than wide** (28×48); no half-pane or dashed line on perimeter icons
- [ ] 8.5 Release on perimeter guide (combined arrow+icon bounds) pins widget correctly
- [ ] 8.6 Non-pinnable drag: no perimeter guides; split cross still works on containers
- [ ] 8.7 Small host window: guides clamp or hide when edge dimension insufficient (no overlap clipping)
- [ ] 8.8 Drag along left edge above/below centered guide icon: auto-hide still activates (full-edge hit band)
- [ ] 8.9 Empty-edge drag within 32px of host border: auto-hide activates without requiring pixel-perfect 8px aim
