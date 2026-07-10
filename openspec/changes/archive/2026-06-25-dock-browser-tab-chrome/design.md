## Context

Blunder docking uses `DockManager` for tree semantics and Slint for chrome. **Main window** (`editor_window.slint`): per-container tabs via `DockTabsLayer` overlaid on `tab_bar_rect`; content areas may show a redundant `panel-label` text. **Native OS floats** (`floating_panel_window.slint` on borderless SDL child HWNDs): a 22px title row (`title-move` + window ×) plus a 26px `DockTabsLayer` row—three visible "Hierarchy" labels for a single-tab float. **In-host floats** (`docking-floatings`): title-only frame without integrated tabs.

Pointer routing for tab drag / re-dock (`tickGlobalDockPointerPoll`, `hideNativeFloatForActiveTabDrag`) is implemented in `dock-float-redock-guide-stability`; this change is **presentation and hit-region** only—C++ drag callbacks stay the same.

## Goals / Non-Goals

**Goals:**

- One **integrated chrome row** per dock container and OS float: browser-style tabs top-left, visually consistent across main host and native floats.
- **Tab drag** is the primary gesture for detach, float preview, OS float commit, and re-dock (ADS/GIF parity).
- **Option B close:** window × (top-right) closes the float container / active close path; tab × closes individual widgets only.
- **Chrome blank-area drag** on OS floats moves the window (replaces separate title-text row).
- Single `chrome_top` height for resize hit-testing (`hitFloatResizeEdge`) and layout metrics.
- Remove duplicate in-content panel titles when tabs are shown.

**Non-Goals:**

- Custom Win32 caption integration (tabs in system title bar); chrome remains client-area Slint on borderless HWNDs.
- Viewport native OS float (unchanged policy).
- Tab pinning / auto-hide chrome changes beyond layout fit.
- Cross-shaped ADS guide widget (keep existing edge/center guide squares).
- Linux/macOS browser-chrome parity beyond best-effort layout.

## Decisions

### 1. New `DockChromeBar` Slint component

**Choice:** Add `DockChromeBar` in `docking_panel.slint` (or sibling) composing:

```
┌─[Tab…][Tab…]────── drag strip ──────[Win ×]─┐  height = chrome_bar_height (~32px)
│  content (no duplicate panel title)          │
└──────────────────────────────────────────────┘
```

- Left: horizontal tab strip (reuse tab drag `TouchArea` logic from `DockTabsLayer`).
- Middle/right: `title-drag` `TouchArea` with `MouseCursor.move` for OS float window move only.
- Right: window-close `TouchArea` (float OS window / in-host float frame).

`DockTabsLayer` remains the tab model + callbacks; `DockChromeBar` wraps it for integrated chrome. Main `DockingHost` replaces bare `DockTabsLayer` overlay + separate float title rectangles with `DockChromeBar` positioned on `tab_bar_rect` / float frame top.

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Only restyle existing two rows | Keeps dual-row confusion |
| Tabs in SDL system title bar | Out of scope; borderless custom chrome already works |

### 2. Wide scope: main host + OS float + in-host float frame

**Choice:** Use `DockChromeBar` in:

1. `floating_panel_window.slint` — replace title row + `DockTabsLayer`.
2. `editor_window.slint` `DockingHost` — tabs on each tile's `tab_bar_rect` via chrome bar (not a separate global-only layer without window close).
3. `docking-floatings` in-host frames — replace title-only `Rectangle` with chrome bar (tabs from model; window × triggers float close path).

Native OS floats hide in-host `docking-floatings` overlay for the same node when `native_os_window` is on (existing sync); chrome must match so switching flag does not change interaction model.

### 3. Close semantics (option B)

**Choice:**

| Control | Callback | Behavior |
|---------|----------|----------|
| Tab × | `widget-closed` / `on_close_widget` | `DockManager::closeWidget(widget_id)` |
| Window × (chrome right) | `close-pressed` / new `window-close` | Close active widget or entire float node per existing `closeWidget` on float (same as today’s float window ×) |

Window × does **not** duplicate tab × on the last tab; it uses the dedicated top-right control. Tab × remains on each tab for per-widget close.

### 4. Drag routing unchanged

**Choice:** No change to `DockManager::beginDrag`, `handleDragMove`, `endDrag`, or `tickGlobalDockPointerPoll`. Tab callbacks fire from chrome bar tab `TouchArea`s with the same dock-local coordinates (`toDockLocal` / `screenToDockLocal`). `title-move-*` fires only from chrome blank strip on floats.

### 5. Layout metrics consolidation

**Choice:** Add `chrome_bar_height` to `DockLayoutMetrics` (default ~32px). Deprecate using `floating_title_height + tab_bar_height` as separate top inset for hit tests:

- `hitFloatResizeEdge`: `k_chrome_top = chrome_bar_height` (single row).
- `appendContainerTile`: `tab_bar_rect.height = chrome_bar_height`; content starts below it.
- `appendFloatingFrame`: `title_rect` spans full chrome row (for in-host float move hit area metadata if still needed).

Keep `tab_bar_height` as alias or remove after migration in implementation tasks.

### 6. Panel header deduplication

**Choice:** Remove `panel-label` `Text` from `DockingHost` content `for tile` blocks and from float content wrappers where `DockChromeBar` shows the active tab title. Hierarchy/Inspector/ContentBrowser internal headers: hide or shrink if they only repeat the tab name (prefer removing the outer duplicate; keep functional sub-headers inside panels if any).

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Narrow float window leaves no blank strip for window move | Enforce `min-width` on chrome bar; blank strip gets `min-width: 24px` |
| Multi-tab overflow | Horizontal clip + future scroll; v1 elide or fixed min tab width |
| `hitFloatResizeEdge` drift if chrome height differs Slint vs C++ | Single constant shared via generated property or documented `constexpr` matching Slint `chrome_bar_height` |
| Re-test `dock-float-redock-guide-stability` 6.3–6.5 | Add explicit validation tasks; update manual steps to "drag top chrome tab" |
| In-host vs native visual mismatch during flag toggle | Same `DockChromeBar` component both paths |

## Migration Plan

1. Implement `DockChromeBar` + wire on OS float (highest user pain).
2. Switch main `DockingHost` tabs to chrome bar; remove content `panel-label`.
3. Update in-host `docking-floatings` frame.
4. Adjust C++ metrics and `hitFloatResizeEdge`.
5. Manual validation: tab drag float/re-dock, window ×, tab ×, chrome strip move, resize cursors.

Rollback: feature flag not required; revert Slint + metrics commit. No data migration.

## Open Questions

- Exact `chrome_bar_height` (32 vs 34px) — pick one constant in implementation and match Slint.
- Whether in-host float frames need window × when `native_os_window` is off (yes for consistency per wide scope).
