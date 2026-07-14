## Why

Editor UI still mixes hand-drawn dock chrome glyphs with emoji/Unicode text icons (search, refresh, folders, tree/breadcrumb arrows, Inspector scale-link). Emoji and many symbols fail or look inconsistent under Slint’s bundled font, while Godot already ships a coherent MIT `editor/icons` SVG set that matches the Godot-like Inspector direction. Vendoring that pack once lets Blunder replace current icon affordances and reuse the same glyphs later without depending on a local `Dev/godot` checkout.

## What Changes

- Vendor Godot `editor/icons/*.svg` into `engine/3rdparty/godot-icons/` with a short LICENSE/README (MIT attribution).
- Keep the full pack on disk; compile into the editor binary **only** SVGs referenced by Slint `@image-url`.
- Replace dock close/pin with Godot SVGs (`GuiClose`, `Pin`) via existing dedicated Slint icon components; keep minimize as hand-drawn geometry when no clean minus glyph exists.
- Replace Content Browser emoji/Unicode icons (search, refresh, folder, tree expand arrows, breadcrumb separators) with Godot SVGs + `Image.colorize`.
- Replace Inspector Scale link chain emoji with Godot `Instance` / `Unlinked`; replace transform section expand chevrons with tree arrow SVGs.
- Preserve hit targets, callbacks, and layout behavior; visual/source change only.

## Capabilities

### New Capabilities

- `godot-editor-icons`: Vendored Godot editor SVG icon pack, dedicated Slint icon components that load and colorize referenced glyphs, and their use on dock chrome, Content Browser, and Inspector affordances listed above.

### Modified Capabilities

- `dock-chrome-icons`: Close and pin SHALL use Godot SVGs (`GuiClose`, `Pin`) with colorize; minimize may remain vector geometry when no suitable Godot minus icon exists. Font-independence requirement remains.

## Impact

- **Assets**: new `engine/3rdparty/godot-icons/` (~1037 SVGs + LICENSE/README).
- **Slint**: `dock_chrome_icons.slint`, new helper icon components (browser/inspector), edits to `docking_panel.slint`, `content_browser.slint`, `inspector_panel.slint`.
- **Build**: Slint embed of referenced SVGs only; no C++ API changes expected.
- **Docs**: `CONTEXT.md` Editor Icon / Scale-link icons terms (already recorded).
- **Risk**: medium-low — UI-only; verify colorize on stroke+fill SVGs at dock sizes; binary size stays small if only referenced icons embed.
