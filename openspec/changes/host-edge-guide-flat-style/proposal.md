## Why

Host-level four-edge dock guides (`DockHostGuidesLayer`) still render as square `DockGuideButton` icons with half-pane fill and dashed dividers—the same style as container split cross guides. The target ADS reference uses a distinct **flat-wide perimeter icon**: white inward arrow, blue chrome bar, and full light-blue body (no dashed split). That visual already exists in `DockAutoHideGuideButton`, but host guides were explicitly spec'd to avoid it. We need host edge guides to match the reference while keeping host-dock and auto-hide semantics separate via shared visual primitives (path B), not a one-off copy-paste.

## What Changes

- Introduce shared Slint components **`DockEdgeGuideIcon`** and **`DockEdgeGuideButton`** (arrow + flat-wide body) extracted from current `DockAutoHideGuideButton` visuals
- Refactor **`DockAutoHideGuideButton`** to compose the shared primitives (behavior and metrics unchanged)
- Switch **`DockHostGuidesLayer`** from `DockGuideButton` to `DockEdgeGuideButton` with `faint` / `highlighted` support
- Update C++ host guide layout and hit-test rects to flat-wide dimensions (48×28 landscape / 28×48 portrait + arrow), aligned with `computeAutoHideGuideLayouts` geometry
- Add `edge` (or slot→edge mapping) to host guide view data for Slint orientation
- **BREAKING (visual only):** Host four-edge guides no longer match split cross `DockGuideButton` style; container cross guides unchanged

## Capabilities

### New Capabilities

- `dock-edge-guide-visual`: Shared flat-wide edge guide pictograph (arrow, chrome bar, full body fill, portrait/landscape per edge, faint/highlight states) used by multiple dock layers

### Modified Capabilities

- `dock-host-edge-guides`: Replace visual-style requirement (half-pane `DockGuideButton`) with flat-wide `DockEdgeGuideButton`; update icon bounds / hit rect to include arrow-inclusive layout

## Impact

- **Slint:** `docking_panel.slint` — new shared components; `DockHostGuidesLayer`, `DockAutoHideGuideButton` refactors; `DockGuideMetrics` / `DockAutoHideGuideMetrics` may consolidate shared tokens into `DockEdgeGuideMetrics`
- **C++:** `dock_guide_hit.cpp` (`computeHostGuideLayouts`, `hitHostGuideSlot`), `dock_manager.cpp` (`appendHostGuides`), `dock_layout_model.h` (host guide fields if needed)
- **Bridge:** `slint_system.cpp` — sync any new/changed model fields
- **Specs:** `openspec/specs/dock-host-edge-guides/spec.md` visual requirement superseded
- **Unchanged:** Container cross guides (`DockGuidesLayer` / `DockGuideButton`), auto-hide edge-zone drag semantics, `dockToRoot` half-window preview behavior
