## 1. Shared Slint primitives (path B)

- [x] 1.1 Add `DockEdgeGuideMetrics` global in `docking_panel.slint` (arrow-size, arrow-gap, icon-width/height, chrome-height, body-opacity, faint-opacity); alias or re-export from `DockAutoHideGuideMetrics`
- [x] 1.2 Extract `DockEdgeGuideArrow` from `DockAutoHideGuideArrow` (same Path commands per edge 0–3)
- [x] 1.3 Extract `DockEdgeGuideIcon` from `DockAutoHideGuideIcon` (chrome bar + full body fill; frame highlight)
- [x] 1.4 Add `DockEdgeGuideButton` with edge orientation, `highlighted`, and `faint` properties (arrow + icon layout logic from current `DockAutoHideGuideButton`)

## 2. Refactor auto-hide guide to compose shared primitives

- [x] 2.1 Refactor `DockAutoHideGuideButton` to delegate to `DockEdgeGuideButton` (keep export name for `DockAutoHideGuidesLayer` compat)
- [x] 2.2 Remove duplicate layout/body code from old `DockAutoHideGuideIcon` / `DockAutoHideGuideArrow` (delete or thin-alias)
- [x] 2.3 Verify `DockAutoHideGuidesLayer` still compiles and renders pixel-parity for all four edges (if layer is exercised in dev)

## 3. Host guide layer visual switch

- [x] 3.1 Update `DockHostGuidesLayer` to render `DockEdgeGuideButton` instead of `DockGuideButton`
- [x] 3.2 Add slot→edge mapping in Slint (left=0, right=1, top=2, bottom=3); pass `faint` and `highlighted` from `DockHostGuide`
- [x] 3.3 Confirm `DockGuidesLayer` / container cross guides still use `DockGuideButton` unchanged

## 4. C++ layout and hit-test

- [x] 4.1 Update `computeHostGuideLayouts` to arrow-inclusive rects (48×39 top/bottom, 39×48 left/right) using `auto_hide_guide_*` metrics; consider shared helper with `computeAutoHideGuideLayouts`
- [x] 4.2 Update `hitHostGuideSlot` to use new layout rects (full component bounds including arrow)
- [x] 4.3 Add sync comment in `dock_manager.h` tying C++ `auto_hide_guide_*` fields to `DockEdgeGuideMetrics`
- [x] 4.4 Confirm `appendHostGuides` / `buildLayoutModel` emit correct rects without model struct changes

## 5. Bridge and validation

- [x] 5.1 Rebuild editor; fix any Slint compile errors from renamed metrics/components
- [ ] 5.2 Manual test: drag tab within 48px of each host edge — four faint flat-wide guides appear
- [ ] 5.3 Manual test: hover each host guide — highlight + half-window preview + `dockToRoot` on drop
- [ ] 5.4 Manual test: container cross guides still show half-pane dashed style when hovering container interior
- [ ] 5.5 Manual test: auto-hide edge zone still works (strip preview) without host guide false positives outside icon bounds
