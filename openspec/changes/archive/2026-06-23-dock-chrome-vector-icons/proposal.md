## Why

Dock tab and auto-hide chrome buttons (close ×, pin 📌, minimize –) render as empty rectangles (missing-glyph tofu) on Windows because `docking_panel.slint` uses Unicode symbols and emoji that Slint's bundled UI font does not include. Tab titles (ASCII) render correctly; only icon glyphs fail. This was reported after the docking-auto-hide feature shipped and blocks users from recognizing close/pin controls.

## What Changes

- Add reusable Slint vector icon components for dock chrome: close (×), pin, and minimize (–).
- Replace all `Text` nodes that render `\u{2715}`, `\u{1F4CC}`, and `\u{2013}` in dock UI with the new components.
- Icons SHALL use geometry (lines/rectangles), not font glyphs, so they render identically across platforms and DPI.
- Hover colors SHALL match current dock chrome behavior (muted default, bright on hover).
- Scope is dock chrome only in this change; Content Browser emoji/icons are out of scope unless trivial to reuse patterns.

## Capabilities

### New Capabilities

- `dock-chrome-icons`: Vector-drawn dock chrome glyphs (close, pin, minimize) and their integration into tab, auto-hide strip, and overlay title bars.

### Modified Capabilities

- _(none)_ — visual rendering fix; docking-auto-hide behavior and APIs unchanged.

## Impact

- **Slint UI**: new `dock_chrome_icons.slint` (or equivalent); edits to `docking_panel.slint`.
- **C++ / dock manager**: no API or layout model changes expected.
- **Build**: `engine_runtime` / `engine_editor` Slint compile only.
- **Risk**: low — UI-only; hit targets and callbacks remain the same.
