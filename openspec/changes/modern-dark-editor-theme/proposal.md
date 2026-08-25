## Why

Editor chrome is still a mix of hardcoded hex, `std-widgets` Buttons, and leftover Godot density. Authors need one modern dark Editor Theme (grilled into ADR 0039) so Application Bar, docks, controls, and dialogs read as one product without rewriting Hierarchy row geometry, Inspector property-field rhythm, or Content Browser / Project Manager information architecture.

## What Changes

- Introduce named Editor Theme tokens as the source of truth: three **Base layers**, hairline **Editor depth** (no inset/outset bevels), one **Editor accent** (`#4C8DFF`) with derived soft/line/hover tints, **Editor corner radius** (6 / 10 / 14 px), Inter 12px / 22px single-line
- Replace Application Bar with Base 1 ghost Save/Undo (Redo/Save As) left, accent Play in a centered segmented cluster, View right — no native File/Edit menus this pass
- Restyle dock frames, pill tabs, splitters, Viewport tool strips (still overlays), Content Browser / History chrome, and authored **Editor modals** with Editor controls
- Ship a Slint Editor controls kit whose *inventory* follows Unity Foundations; look is Editor Theme, not Unity 2022 Dark
- Inspector: Foldout / Add… / Remove use Editor controls; **Inspector property fields** (FOV, Light, Behaviour bag, AxisNumberField) stay Godot compact cells with recessed theme fill
- Hierarchy: freeze 22px rows, gutter, Hierarchy Line `#737373`, square selection rectangle; selected-row *color* follows Editor accent
- Project Manager: keep Godot spatial rhythm; colors and buttons/modals follow Editor Theme
- Recolor Godot Editor Icons to theme icon gray (`#B3BBC4`); checked tools tint with accent
- Static preview remains `docs/previews/editor-theme-unity-dark.html` (Modern default; Unity Dark toggle is comparison-only)

Out of this change: Light theme; Unity Editor Icon Library; moving Viewport tool strips onto the App Bar or a Scene Window Toolbar; restyling Hierarchy Line / row geometry / Add… IA; replacing AxisNumberField with Numeric Field.

## Capabilities

### New Capabilities
- `editor-theme`: named tokens, Base layers, accent, corner radius, depth, and brightness anchored to Godot panel/field values
- `editor-controls`: Slint control kit inventory, metrics, states, and the rule that Inspector property fields are not these controls
- `editor-shell`: Application Bar layout, dock frames and pill tabs, Viewport tool strips as restyled overlays, Editor modal chrome

### Modified Capabilities
- `hierarchy-panel`: selected-row color derives from Editor accent; row geometry, Hierarchy Line, and square selection stay frozen
- `inspector-transform`: Transform header may use Editor controls Foldout; vector cells stay compact AxisNumberField with recessed theme fill
- `inspector-add-menu`: Add… / Remove chrome uses Editor controls; grouping, uniqueness, and cascade stay as specified
- `project-manager-chrome`: palette, buttons, and Create/Import dialogs use Editor Theme / Editor controls / Editor modal; Godot layout unchanged
- `dock-browser-tab-chrome`: dock tab well uses Editor Theme pill tabs on Base 1; tab-drag and float-move gestures unchanged

## Impact

- Slint: `editor_window.slint`, `docking_panel.slint`, `floating_panel_window.slint`, `hierarchy.slint`, `inspector_panel.slint`, `axis_number_field.slint`, `content_browser.slint`, `history_panel.slint`, `transform_toolbar.slint`, `animation_preview_toolbar.slint`, `project_manager.slint`, authored dialogs (`play_dirty_scene_dialog`, `open_dirty_scene_dialog`, `import_mesh_dialog`, `detection_reimport_dialog`, `browser_delete_dialog`)
- New Slint theme + control primitives (likely `editor_theme.slint` / `editor_controls.slint` or equivalent)
- Bindings: `slint_system` only as needed to restyle; no command/history model changes
- Docs: ADR 0039 and `CONTEXT.md` Editor chrome already written; glossary avoid-line for Hierarchy selection color needs to match accent (tiny cleanup)
- Preview: `docs/previews/editor-theme-unity-dark.html` is the visual contract
- Tests: existing Hierarchy / Inspector / PM tests stay green; add token/unit tests only if a C++ token table is introduced
- Out of scope systems: Vulkan Editor Overlays, Navigate gizmo math, Player
