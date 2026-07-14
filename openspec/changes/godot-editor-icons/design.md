## Context

Dock chrome already uses hand-drawn Slint rectangles in `dock_chrome_icons.slint` (close/pin/minimize) after `dock-chrome-vector-icons`. Content Browser and Inspector still use emoji/Unicode (`🔍`, `⟳`, `📁`/`📂`, tree `▸`/`▾`, breadcrumb `›`, scale-link `⛓`, transform header chevrons). Godot’s MIT `editor/icons` (~1037 SVGs) live at `E:\Dev\godot\editor\icons` for local reference; Blunder must not require that path at build time.

Grilled decisions (2026-07-13): scope = dock + emoji/Unicode affordances (not Move/Rotate/Scale toolbar text); vendor full `editor/icons` only; Image + `colorize`; dedicated components; compile only referenced SVGs; scale-link = `Instance`/`Unlinked`; minimize stays geometry if no clean minus.

## Goals / Non-Goals

**Goals:**

- Vendor Godot `editor/icons` under `engine/3rdparty/godot-icons/` with short LICENSE/README.
- Dedicated Slint components wrapping `@image-url` + `colorize` for close, pin, search, reload, folder, tree arrows, breadcrumb arrow, scale-link pair.
- Wire those into dock chrome, Content Browser, and Inspector without changing callbacks/hit targets.
- Keep minimize as existing rectangle geometry.

**Non-Goals:**

- Transform toolbar text → icons (Move/Rotate/Scale/Global).
- Vendoring `scene/theme/icons`.
- Embedding all ~1037 SVGs into the binary.
- Runtime theme pack / icon name registry / hot-reload of icons.
- Changing C++ Content Browser / Inspector APIs.

## Decisions

### 1. Vendor path `engine/3rdparty/godot-icons/`

**Choice:** Copy all `*.svg` from Godot `editor/icons/` into `engine/3rdparty/godot-icons/` (flat, preserve Godot filenames). Add `LICENSE` (MIT text from Godot) + `README.md` (upstream path, purpose, “only referenced SVGs embed”).

**Alternatives:** Live-read `../godot` (breaks CI); submodule (heavier); only copy used files (conflicts with “pack for later”).

### 2. Relative `@image-url` from Slint modules

**Choice:** From `engine/src/runtime/function/slint/*.slint`, reference  
`@image-url("../../../../3rdparty/godot-icons/<Name>.svg")`.  
Slint embeds only URLs that appear in compiled UI.

**Alternatives:** Duplicate used icons next to Slint (two copies); CMake copy step into build tree (extra moving parts for v1).

### 3. Dedicated components keep `icon-color` / `icon-size` API

**Choice:** Keep `DockChromeCloseIcon` / `DockChromePinIcon` public API; internals become `Image { source: …; colorize: icon-color; width/height: icon-size }`. Add sibling components in the same file or `editor_icons.slint` for browser/inspector glyphs. `DockChromeMinimizeIcon` stays rectangle geometry.

**Alternatives:** Generic `EditorIcon { name }` (deferred); inline `Image` at every call site (noisy).

### 4. Icon mapping (locked)

| Affordance | Godot SVG |
|------------|-----------|
| Dock close / unpin | `GuiClose.svg` |
| Dock pin | `Pin.svg` |
| Dock minimize | hand-drawn (no clean minus) |
| Search | `Search.svg` |
| Refresh | `Reload.svg` |
| Folder (tree + grid dir) | `Folder.svg` |
| Tree collapsed / expanded | `GuiTreeArrowRight.svg` / `GuiTreeArrowDown.svg` |
| Breadcrumb separator | `GuiTreeArrowRight.svg` (or `ArrowRight.svg` if clearer at size) |
| Scale link on / off | `Instance.svg` / `Unlinked.svg` |
| Inspector Transform section chevron | same tree arrows |

### 5. Colorize contract

**Choice:** Parent binds `icon-color` as today; component sets `Image.colorize`. Godot icons are mostly `#e0e0e0`/`#fff` fill/stroke so modulation works.

**Fallback:** If a specific SVG colorizes poorly, prefer a different Godot glyph in the same semantic family before reverting to geometry (except minimize, already geometry).

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| `colorize` weak on mixed stroke/fill SVGs | Visual check at 100%/125% DPI; swap glyph or keep geometry for that control |
| Large git checkout from ~1037 SVGs | Accept; icons are small text; document in README |
| Accidental `@image-url` of many unused icons | Code review: only mapped names; no glob embed |
| Relative path breaks if Slint file moves | Document path depth in README; keep icon components under `function/slint/` |
| `dock-chrome-icons` spec still says “geometry” | Delta MODIFIED requirements to allow Godot SVG + colorize |

## Migration Plan

1. Copy icons + LICENSE/README into `engine/3rdparty/godot-icons/`.
2. Update/add Slint icon components; wire call sites.
3. Build `engine_editor` Debug; visual QA listed surfaces.
4. Archive OpenSpec change; sync `openspec/specs/dock-chrome-icons` and new `godot-editor-icons` on archive.

Rollback: revert Slint + remove `godot-icons` directory (or leave vendored pack unused).

## Open Questions

- None blocking. Breadcrumb may use `ArrowRight` vs `GuiTreeArrowRight` after a quick size check during apply.
