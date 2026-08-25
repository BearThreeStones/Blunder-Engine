## 1. Theme tokens

- [x] 1.1 Add `editor_theme.slint` with `global EditorTheme`: Base 1/2/3, recessed field, hairline, text/icon/button/titlebar, accent `#4C8DFF` plus derived soft/line/hover, radii 6/10/14, line 22px / large 26px
- [x] 1.2 Import the global from Editor Session, floating panel host, and Project Manager roots; set window backgrounds to Base 1 / Window
- [x] 1.3 Fix `CONTEXT.md` Editor accent _Avoid_ so Hierarchy selection is allowed to follow accent (structure freeze only)

## 2. Application Bar and dock chrome

- [x] 2.1 Rebuild Application Bar (~48px): ghost Save / Undo / Redo / Save As left, centered Play/Pause/Stop cluster with accent Play, View right — no `std-widgets`
- [x] 2.2 Restyle dock frames (Window fill, hairline, 10px radius, clip) without changing `DockChromeMetrics.bar-height` / C++ `chrome_bar_height` unless pills clip
- [x] 2.3 Pill dock tabs on App Bar ground; recolor close/pin to theme icon gray; keep tab-drag and float-move gestures
- [x] 2.4 Measure pill tabs at 32px chrome height; only then consider a coordinated height bump

## 3. Editor controls kit

- [x] 3.1 Add `editor_controls.slint` (or split): Button, ToolbarButton, Tab, Foldout, Text/Search Field, Toggle, Slider — hairline, 6px, 22px, accent primary/checked/focus, disabled muted
- [x] 3.2 Add Color Field and Object Field primitives (empty / assigned / drop-target) even if first consumers are later panels
- [x] 3.3 Stop using `std-widgets` Button / LineEdit / CheckBox / Slider in Application Bar (already 2.1) as the template for later swaps

## 4. Editor modals

- [x] 4.1 Shared modal chrome: 14px radius, no titlebar divider, dim overlay, actions bottom-right, one accent primary
- [x] 4.2 Apply to Play dirty, Open dirty, Import Mesh, Detection reimport, Browser Delete — copy and button sets unchanged
- [x] 4.3 Apply to Project Manager Create/Import dialogs

## 5. Unfrozen panels

- [x] 5.1 Content Browser chrome (toolbar, search, tree, grid/Details, thumbnails) uses Editor controls; IA, Pull, virtual paths unchanged; selected thumb uses accent outline
- [x] 5.2 History panel chrome uses Editor controls; filters/rows/grouping unchanged
- [x] 5.3 Viewport tool strips (transform, projection, animation preview) stay overlays; floating Toolbar look; checked tool tints with accent
- [x] 5.4 Project Manager: Godot layout kept; list rows as hairline cards with accent selection; New Project / Open accent primaries; remaining `std-widgets` removed

## 6. Inspector and Hierarchy interiors

- [x] 6.1 Inspector Foldouts, Add… button/popup chrome, and Remove use Editor controls; Add… grouping/uniqueness/cascade unchanged
- [x] 6.2 Inspector property fields (FOV, Light, Behaviour bag, SkeletonModifier) and AxisNumberField keep 22px Godot cells; cell fill = recessed theme field (not Numeric Field)
- [x] 6.3 Hierarchy selected row uses accent-soft fill + accent-tinted text; keep 22px rows, gutter, square selection, Hierarchy Line `#737373`
- [x] 6.4 Recolor remaining Godot SVG chrome icons to `#B3BBC4`; checked tools colorize with accent

## 7. Validation

- [x] 7.1 Grep editor Slint (except Animation Tree Canvas follow-up) for leftover `std-widgets` Button/LineEdit/CheckBox/Slider and chrome hex that bypasses `EditorTheme`
- [x] 7.2 Build `engine_editor` and `project_manager`; run existing Hierarchy / Inspector / editor-commands tests
- [ ] 7.3 Manual: App Bar Play cluster, dock tab drag still floats, dirty Play modal primary, Hierarchy accent selection vs square row, Camera FOV still compact cell, PM Open primary
