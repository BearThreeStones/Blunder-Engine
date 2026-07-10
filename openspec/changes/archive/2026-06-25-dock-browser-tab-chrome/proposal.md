## Why

Native OS floating panels and the main docking host currently use a **two-row chrome** (separate title bar + tab bar), which duplicates panel names three times for single-tab floats and splits user intent: title bar moves the window, tab bar re-docks. Users expect **browser-style tabs** in the top chrome (top-left), with **tab drag** as the single gesture for float, move-as-group, and re-dock—matching ADS/Qt docking demos and reducing confusion during `dock-float-redock-guide-stability` validation.

## What Changes

- Introduce a shared **browser-style dock chrome bar**: tabs anchored top-left in one row with the window/panel header; eliminate the separate 22px title row above `DockTabsLayer` on OS floats.
- **Wide scope:** apply the same chrome model to **main-window dock containers** (`editor_window.slint` / `DockingHost`) and **native OS float windows** (`floating_panel_window.slint`); align in-host float frames (`docking-floatings`) where they still render.
- **Drag semantics:** dragging a **tab** starts `beginDrag` / float / re-dock (unchanged C++ paths); dragging the **empty chrome strip** to the right of tabs moves the floating window (replaces dedicated title-drag on tab text duplicate).
- **Close semantics (option B):** top-right **window close** closes the entire float / last-widget behavior via existing `closeWidget`; per-tab **×** on each tab closes only that widget (unchanged dock close path).
- Remove **duplicate in-content panel titles** (e.g. "Hierarchy" label inside content when a tab already shows the name) for docked and floated panels.
- Update **resize hit-testing** (`hitFloatResizeEdge`) and layout metrics so top chrome is a **single row height** instead of `title_height + tab_bar_height`.
- **BREAKING (visual):** OS float and docked panel chrome layout changes; screenshots and manual test steps that reference "second row tab bar" are obsolete.

## Capabilities

### New Capabilities

- `dock-browser-tab-chrome`: Unified top chrome row with browser-style tabs (top-left), tab-drag for dock/float/re-dock, and chrome blank-area window move on floats.
- `dock-chrome-close-controls`: Window-level close (top-right) vs per-tab close (option B) on integrated chrome.
- `dock-panel-header-dedup`: Suppress redundant in-content panel title labels when tab chrome is visible.

### Modified Capabilities

- (none in `openspec/specs/` — docking chrome requirements live in change deltas only)

## Impact

- **Slint:** `docking_panel.slint` (new or extended `DockChromeBar`), `floating_panel_window.slint`, `editor_window.slint` (`DockingHost`, `docking-floatings` frames).
- **C++:** `dock_floating_window_host.cpp` (callback wiring, chrome height), `slint_system.cpp` (`hitFloatResizeEdge`, layout sync), `dock_manager.cpp` / `DockLayoutMetrics` (single `chrome_bar_height` or equivalent).
- **Validation:** Re-run OS float re-dock (6.3), cancel drag (6.4), resize cursor (6.5) from `dock-float-redock-guide-stability` after chrome lands.
- **Depends on:** existing docking tree, `DockFloatingWindowHost`, and pointer-routing work from `dock-floating-os-window-drag-preview` / `dock-float-redock-guide-stability` (logic preserved; chrome is presentation + hit regions).
