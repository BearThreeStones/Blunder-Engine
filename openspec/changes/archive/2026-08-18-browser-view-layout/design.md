## Context

See proposal.md for motivation. Today `content_browser.slint` always draws a thumbnail grid; the status-bar `Slider` maps 48–128px onto `thumb-size` (default 64px on `editor_window.slint`, 80px on the component). `ContentEntry` already has `size_bytes` and `modified_time` from `ContentIndex::scan` (Assets-root file, not Intermediate body). `ContentBrowserGridItem` / Slint `BrowserGridRow` do not surface type, size, or date. Grid item pointer handling is Slint `TouchArea` per tile (path callbacks), so Details can reuse those callbacks. `thumb-size` is already shared across docked `FilesystemHistoryHost` instances via `editor_window`; OS-float `floating_panel_window.slint` does not bind `thumb-size` today.

## Goals / Non-Goals

**Goals:**

- One Slint View Layout state (Details vs icons + continuous thumb size) shared by slider and menu.
- Details table UI with type-keyed Editor Icons; C++ fills type/size/date and owns sort.
- Bind layout through `editor_window` / `FilesystemHistoryHost` the same way as `thumb-size`; also bind it on `floating_panel_window` so a floated Browser does not fork size.

**Non-Goals:**

- Editor-preference persistence or per-folder views.
- Column resize, Explorer List/Tiles/Content, Ctrl+wheel size, type-icon pack beyond the six Godot glyphs.
- Changing thumbnail generation, drag/drop semantics, or delete.

## Decisions

### 1. Layout state lives in Slint; sort and row metadata live in C++

**Choice:** `in-out property <bool> details-view` plus existing `thumb-size` on `ContentBrowser`, lifted to `editor_window` (`browser-details-view` + `browser-thumb-size`) and bound through `FilesystemHistoryHost` and `floating_panel_window`. `ContentBrowserSystem` owns sort column + direction and fills per-item type/size/date on `ContentBrowserGridItem`.

**Why:** Slider/menu are pure chrome; sort must apply to the shared `gridItems()` model so icon and Details stay in one order. Alternatives: all state in C++ (extra sync for every slider tick); all sort in Slint (duplicates `rebuildGrid` ordering).

### 2. Slider range 0–128; below 48 is Details

**Choice:** Slider `minimum: 0`, `maximum: 128`. `value < 48` ⇒ `details-view = true` (display value 0 while in Details). `value >= 48` ⇒ icons at that pixel size. Leaving Details via slider jumps to Small Icons (48). Menu Details sets slider to 0; icon menu items set 48 / 80 / 104 / 128.

**Why:** Matches Unity min-detent plus continuous icon scaling without a second control. Alternatives: discrete 5-step slider (loses 48–128 continuity); restore last icon size when leaving Details (conflicts with grilled “first stop is Small Icons”).

Nearest-preset check: distances to 48, 80, 104, 128; ties prefer the larger (so default 64px checks Medium Icons).

### 3. Type from path suffix, not registry

**Choice:** Classify in one helper used by `rebuildGrid` / Slint push:

| Kind | Rule | Type label | Godot SVG |
|------|------|------------|-----------|
| Folder | `is_directory` | Folder | `Folder.svg` (existing `EditorIconFolder`) |
| Mesh | `.mesh.yaml` / `.mesh.asset` | Mesh | `Mesh.svg` |
| Scene | `.scene.asset` | Scene | `PackedScene.svg` (existing `EditorIconScene`) |
| Texture | `.texture.yaml` | Texture | `Texture2D.svg` |
| AnimationClip | `.animation.yaml` | AnimationClip | `Animation.svg` |
| File | otherwise | File | `File.svg` |

**Why:** Same suffix rules as registry/import; no YAML parse. AnimationTree / materials / README fall through to File this slice.

Slint: add `type-kind: int` (0–5) and formatted `type-label`, `size-text`, `date-text` on `BrowserGridRow`. Details picks the Editor Icon with `if type-kind == …`.

### 4. Format Size and Date in C++

**Choice:** Push display strings. Size: empty for folders; otherwise binary KB/MB (e.g. `12.9 KB`) from `size_bytes`. Date: empty for folders or `modified_time == 0`; otherwise local `YYYY-MM-DD HH:MM` via `clock_cast` from `file_clock`. Sort uses raw `size_bytes` / `modified_time` / type label / display name, not the formatted strings.

**Why:** Slint should not reimplement file_clock. Alternative: integer fields + Slint format (date conversion still needs C++).

### 5. Details UI: sticky header + one-column Flickable

**Choice:** Header row (clickable labels) sits above the item `Flickable` (does not scroll away). Rows are ~22px, leading icon 16px. Name stretches; Type / Size / Date modified fixed widths. `grid-column-count` is 1 in Details so viewport height math uses row height, not thumb cells. Same `grid-select` / `item-press|move|release` / context menu as tiles.

**Why:** Explorer-like frozen headers without a table widget. Alternative: headers inside the scroll viewport (they disappear).

### 6. View Layout popup reuses the context-menu overlay pattern

**Choice:** Status-bar button (Godot `FileList.svg` as `EditorIconViewLayout`) toggles a small overlay menu with five English items and a check on the current/nearest layout. Dismiss on outside click, same as the existing browser context menu. Do not add View items to asset/empty context menus.

### 7. Editor Icon components

**Choice:** Add `EditorIconMesh`, `EditorIconTexture`, `EditorIconAnimation`, `EditorIconFile`, `EditorIconViewLayout` beside existing icons in `editor_icons.slint` (`@image-url` + `colorize`). Reuse `EditorIconFolder` and `EditorIconScene`.

### 8. Tests

**Choice:** Extend `content_browser_test` (or a small sibling) for type classification, Size/Date blank on folders, and folders-first sort by Name/Type/Size/Date including reverse. No Slint screenshot tests.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Slider dead zone 1–47 flickers | While Details, bind slider value to 0; only leave Details at `>= 48` |
| `file_clock` date formatting is fiddly on MSVC | Isolate in one helper; empty string on conversion failure |
| Two Browser hosts (dock + float) diverge | Bind `thumb-size` and `details-view` on `floating_panel_window` like the main window |
| Details row hit targets vs drag | Reuse tile `TouchArea` callbacks; keep `interactive: !layout-drag-active` on the Flickable |
| Extra `@image-url` SVGs grow the binary | Five new referenced glyphs only |

## Migration Plan

1. Land C++ row fields + sort + tests (grid still name-sorted by default).
2. Land Slint Details + slider/menu; keep default 64px icon layout.
3. Wire `editor_window` / history host / floating window bindings.
4. Manual smoke: slider min ↔ Details, menu presets, sort, drag/open/delete from Details.

No rollback beyond revert; session-only state.
