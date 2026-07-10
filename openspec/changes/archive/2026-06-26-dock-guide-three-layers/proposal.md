## Why

Blunder's current drag-drop guides conflate **host-level edge docking**, **auto-hide pin**, and **container split** into overlapping hit bands and always-visible perimeter icons. This causes ADS-parity gaps (half-window edge previews), confusing overlap with the inner cross, and unreliable auto-hide triggering along full window edges. Qt ADS separates three layers; we need the same model with user-confirmed interaction: host guides on all four edges, auto-hide via edge zones only, and hover-driven guide visibility.

## What Changes

- Introduce **three-layer drag-drop model** with explicit hit priority: host edge guide icon → auto-hide edge zone → container cross (inside outer margin only).
- Add **host edge dock guides** (L/R/T/B centered on `m_host_rect`): drop docks widget to **half the workspace** via `dockToRoot`; preview and highlight only when pointer hovers the icon; all four icons **fade in near the host perimeter** during drag.
- **Decouple auto-hide drag** from perimeter guide icons: pin on **thin edge mouse zone** on all four edges (pinnable widgets); narrow strip preview only; no full-edge guide hit bands.
- Change **container split cross** to **hover-icon-only** visibility (five guides faint when eligible, one highlighted under cursor); preview remains ~1/3 container.
- Add **outer margin** inset around `m_host_rect` where container cross hit-testing is suppressed (ADS `DockAreaOuterDistance` analogue).
- **BREAKING (drag UX):** Perimeter `DockAutoHideGuideButton` icons no longer drive auto-hide drop or host dock; `hitAutoHideGuideEdge` full-edge bands removed from drag path.
- **Prerequisite:** Reliable in-host drag pointer routing (global poll or Slint capture layer from `dock-auto-hide-drag-drop` §7.20) so edge zones work along full edge length.
- Supersedes overlapping decisions in `dock-auto-hide-drag-drop` D1/D2 (perimeter icons = auto-hide + always visible + full-edge hit bands).

## Capabilities

### New Capabilities

- `dock-host-edge-guides`: Host-level L/R/T/B guide icons, proximity fade-in, hover highlight, half-window preview, `dockToRoot` drop.
- `dock-guide-hit-layers`: Three-layer hit resolution, outer margin, guide visibility rules (host + cross hover-only), drag controller state extensions.

### Modified Capabilities

- `docking-auto-hide`: Drag-to-pin SHALL use edge mouse zones only (all four edges); decoupled from host guide icon hit rects.

## Impact

- **C++:** `dock_manager.cpp` (`handleDragMove`, `buildLayoutModel`, `endDrag`, `appendGuides`), `dock_drag_controller.*`, `dock_auto_hide.cpp` (remove guide-edge hit from drag), `dock_layout_model.h` (host guide views, hover states)
- **Slint:** New `DockHostGuidesLayer`; rework `DockGuidesLayer` / `DockAutoHideGuidesLayer` visibility; `DockAutoHideDropPreviewLayer` unchanged semantics; `editor_window.slint` layer order
- **Bridge:** `slint_system.cpp` sync; pointer capture / global poll
- **Related changes:** `dock-auto-hide-drag-drop` (visual assets may be reused; behavior superseded here)
