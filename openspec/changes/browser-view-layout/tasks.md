## 1. Row metadata and sort (TDD)

- [x] 1.1 Add a testable classifier (directory vs `.mesh.yaml` / `.mesh.asset` / `.scene.asset` / `.texture.yaml` / `.animation.yaml` → Folder / Mesh / Scene / Texture / AnimationClip / File)
- [x] 1.2 TDD: Size/Date display empty for folders; Size is Assets-root `size_bytes`; Date from `modified_time` when non-zero
- [x] 1.3 TDD: folders-first sort by Name, Type, Size, Date, including reverse on second toggle
- [x] 1.4 Extend `ContentBrowserGridItem` with type kind, type label, size/date text, and raw sort keys; apply sort in `rebuildGrid` (default Name ascending)
- [x] 1.5 Add `setGridSort` (or equivalent) on `ContentBrowserSystem` and push new fields from `SlintSystem::syncContentBrowser`
- [x] 1.6 Run `content_browser_test` (extend that target or a sibling wired in `engine/src/tests/CMakeLists.txt`)

## 2. Editor Icons

- [x] 2.1 Add `EditorIconMesh`, `EditorIconTexture`, `EditorIconAnimation`, `EditorIconFile`, `EditorIconViewLayout` in `editor_icons.slint` (`Mesh.svg`, `Texture2D.svg`, `Animation.svg`, `File.svg`, `FileList.svg`)
- [x] 2.2 Keep `EditorIconFolder` and `EditorIconScene` as the Folder / Scene glyphs

## 3. Content Browser UI

- [x] 3.1 Add `details-view` plus slider 0–128 (`< 48` ⇒ Details, `>= 48` ⇒ icons at that px); leaving Details via slider enters 48px Small Icons
- [x] 3.2 Draw Details: sticky Name / Type / Size / Date modified header (click toggles sort), fixed Type/Size/Date widths, stretching Name, 16px type-keyed Editor Icon (not Thumbnail)
- [x] 3.3 Keep icon grid on Content Browser Thumbnails when not in Details; `grid-column-count` is 1 in Details
- [x] 3.4 Status-bar View Layout button (left of slider) with English menu: Extra Large Icons / Large Icons / Medium Icons / Small Icons / Details; check current/nearest preset; no View submenu on item/empty context menus
- [x] 3.5 Reuse grid `grid-select` / `item-press|move|release` / context menu on Details rows

## 4. Host bindings

- [x] 4.1 Lift `details-view` to `editor_window.slint` and bind through `FilesystemHistoryHost` with `thumb-size`
- [x] 4.2 Bind `thumb-size` and `details-view` on `floating_panel_window.slint` so a floated Browser shares layout
- [x] 4.3 Wire Details header clicks to `ContentBrowserSystem` sort and `syncContentBrowser`

## 5. Verify

- [x] 5.1 Build `engine_editor` Debug
- [ ] 5.2 Manual: slider min ↔ Details; menu presets 48/80/104/128; sort headers folders-first; drag/open/delete from Details; thumbnails still in icon layouts
