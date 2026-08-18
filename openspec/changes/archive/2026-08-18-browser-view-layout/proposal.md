## Why

Content Browser can only show a thumbnail grid whose status-bar slider scales icon size (48–128px). Authors cannot scan Type / Size / Date, and there is no Explorer-style View Layout menu. Unity’s Project window already treats the slider’s minimum as a list; Blunder should match that gesture and add discrete View Layout presets.

## What Changes

- Introduce session-wide **Browser View Layout**: Extra Large / Large / Medium / Small Icons, plus Details. Slider and View Layout menu write the same state.
- Slider at minimum shows Details (columnar Name / Type / Size / Date modified). Dragging right re-enters Small Icons. Icon layouts keep **Content Browser Thumbnails** at 48 / 80 / 104 / 128px.
- Details Name column uses type-keyed **Editor Icons** (Folder, Mesh, Scene, Texture, AnimationClip, File), not Thumbnails. Size is the Assets-root file size; folders leave Size and Date empty.
- Clicking a Details header sorts that column (second click reverses); folders stay before files. Column widths are fixed.
- View Layout control lives on the status bar, left of the slider. Item and empty-area context menus do not gain a View submenu.
- List, Tiles, and Content layouts, per-folder remembered views, and editor-preference persistence are out of this change.

## Capabilities

### New Capabilities

- `browser-view-layout`: Content Browser View Layout (icon sizes + Details), shared slider/menu state, Details columns/sort/icons, and session-wide (not persisted) layout

### Modified Capabilities

- (none — no archived Content Browser view spec exists under `openspec/specs/`)

## Impact

- Slint: `content_browser.slint`, `editor_icons.slint`, `editor_window.slint` (`browser-thumb-size` / layout bindings)
- C++: `ContentBrowserGridItem` / Slint `BrowserGridRow` fields (type, size, date); `ContentBrowserSystem::rebuildGrid` sort; `SlintSystem` model push
- Docs: `CONTEXT.md` **Browser View Layout** (already recorded)
- Tests: grid vs Details sort; type classification; slider/menu state mapping
- Risk: medium-low — UI + model fields; selection, drag, and context menus must keep working in Details
