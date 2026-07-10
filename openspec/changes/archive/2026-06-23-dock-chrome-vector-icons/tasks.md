## 1. Vector icon components

- [x] 1.1 Create `engine/src/runtime/function/slint/dock_chrome_icons.slint` with `DockChromeCloseIcon`, `DockChromePinIcon`, and `DockChromeMinimizeIcon` export components
- [x] 1.2 Each component exposes `in property <color> icon-color` and draws geometry only (no `Text` glyphs)
- [x] 1.3 Close icon: two 1 px diagonal bars in a ~10×10 px box; minimize: one horizontal 1 px bar; pin: head + stem from rectangles

## 2. Integrate into docking panel

- [x] 2.1 Import `dock_chrome_icons.slint` from `docking_panel.slint`
- [x] 2.2 Replace tab `close-area` `Text` (`\u{2715}`) with `DockChromeCloseIcon` centered in touch area
- [x] 2.3 Replace tab `pin-area` `Text` (`\u{1F4CC}`) with `DockChromePinIcon`
- [x] 2.4 Replace auto-hide strip `unpin-touch` `Text` with `DockChromeCloseIcon` (smaller size if needed)
- [x] 2.5 Replace overlay title bar minimize and close `Text` nodes with vector components
- [x] 2.6 Preserve existing `TouchArea` sizes, positions, and callbacks unchanged

## 3. Build and visual verification

- [x] 3.1 Build `engine_editor` (Debug) and confirm Slint compiles without errors
- [x] 3.2 Verify Viewport (and other) dock tab close shows × not tofu box
- [x] 3.3 Verify pinnable tab pin icon, auto-hide strip unpin, and overlay minimize/close render correctly with hover brightening
