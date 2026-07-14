## 1. Vendor Godot editor icons

- [x] 1.1 Create `engine/3rdparty/godot-icons/` and copy all `*.svg` from `E:\Dev\godot\editor\icons\` (preserve filenames)
- [x] 1.2 Add `engine/3rdparty/godot-icons/LICENSE` (Godot MIT text) and `README.md` (upstream path, purpose, embed-only-referenced policy)
- [x] 1.3 Verify pack count (~1037) and that `GuiClose.svg`, `Pin.svg`, `Search.svg`, `Reload.svg`, `Folder.svg`, `GuiTreeArrowRight.svg`, `GuiTreeArrowDown.svg`, `Instance.svg`, `Unlinked.svg` exist

## 2. Dock chrome icon components

- [x] 2.1 Rewrite `DockChromeCloseIcon` in `dock_chrome_icons.slint` to load `GuiClose.svg` via `@image-url` + `colorize: icon-color` (keep `icon-color` / `icon-size` API)
- [x] 2.2 Rewrite `DockChromePinIcon` to load `Pin.svg` the same way
- [x] 2.3 Leave `DockChromeMinimizeIcon` as hand-drawn geometry
- [x] 2.4 Confirm `docking_panel.slint` call sites still compile (no API change)

## 3. Shared Editor Icon components for panels

- [x] 3.1 Add `engine/src/runtime/function/slint/editor_icons.slint` with components: `EditorIconSearch`, `EditorIconReload`, `EditorIconFolder`, `EditorIconTreeArrowRight`, `EditorIconTreeArrowDown`, `EditorIconInstance`, `EditorIconUnlinked` (each: `icon-color`, `icon-size`, Image + colorize)
- [x] 3.2 Export components for import from Content Browser and Inspector

## 4. Content Browser wiring

- [x] 4.1 Import `editor_icons.slint` in `content_browser.slint`
- [x] 4.2 Replace search-box `🔍` Text with `EditorIconSearch`
- [x] 4.3 Replace refresh `⟳` Text with `EditorIconReload`
- [x] 4.4 Replace tree folder `📁` with `EditorIconFolder`
- [x] 4.5 Replace grid directory `📂` with `EditorIconFolder`
- [x] 4.6 Replace tree expand/collapse Unicode arrows with `EditorIconTreeArrowRight` / `EditorIconTreeArrowDown` based on `row.expanded`
- [x] 4.7 Replace breadcrumb `›` separators with `EditorIconTreeArrowRight` (or `ArrowRight` if size check prefers it)
- [x] 4.8 Replace status/thumb-size `🔍` with `EditorIconSearch`

## 5. Inspector wiring

- [x] 5.1 Import `editor_icons.slint` in `inspector_panel.slint`
- [x] 5.2 Replace Scale link `⛓` with `EditorIconInstance` when linked and `EditorIconUnlinked` when not
- [x] 5.3 Replace Transform section `▾`/`▸` with tree arrow Editor Icons

## 6. Build and visual verification

- [x] 6.1 Build `engine_editor` Debug (`cmake --build build/vs2026-debug --config Debug --target engine_editor`)
- [ ] 6.2 Visual check: dock tab close/pin, auto-hide unpin/overlay close+minimize, Content Browser search/refresh/folders/tree/breadcrumb, Inspector scale-link + Transform chevron
- [ ] 6.3 Confirm hover colorize still brightens icons; hit targets still fire the same callbacks
