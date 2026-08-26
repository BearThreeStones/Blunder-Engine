# Modern dark Editor Theme with Godot icons and Godot Inspector value cells

Editor chrome uses a modern dark visual language: slightly cool neutrals in three **Base layers** (Application Bar `#191B1F`, Window `#2A2D31`, Toolbar / raised `#34383D`, recessed fields `#1E2125`) whose brightness is anchored to Godot's editor (panel `#292929`, field `#1C1C1C`) rather than pushed toward black, depth from 1px hairlines (`#3B3F45`) plus soft shadows instead of bevel borders, one **Editor accent** token (`#4C8DFF`) for primary / checked / selected / focus states and Play, **Editor corner radius** (6px controls, 10px window frames and viewport strips, 14px modals), Inter 12px with 22px single-line controls, ghost Application Bar buttons around a segmented Play cluster, pill dock tabs, and translucent floating viewport strips. The control *inventory* still follows Unity Foundations (Button, ToolbarButton, Tab, Foldout, Text / Numeric / Search Field, Toggle, Slider, Color Field, Object Field, List View, Tree View and their states); Unity's palette, bevels, and square corners are not copied.

We are neither a Unity clone nor a Godot clone: Editor Icons stay Godot SVGs recolored to theme icon gray; Hierarchy keeps its authored *structure* (22px rows, gutter, Hierarchy Line `#737373`, square selection rectangle) while its selected row derives from the accent token like every other selection; the Inspector keeps Godot-style **Inspector property fields** (Camera FOV / Near / Far, Light scalars, Behaviour bag) and Transform cells (AxisNumberField, Scale-link), with cell fill recessed under the panel surface; viewport tool strips stay overlays; Content Browser, History, and Project Manager keep their information architecture (Project Manager keeps Godot spatial rhythm). Light theme is out of this decision.

## Considered Options

- **Unity Editor Dark palette with inset/outset bevel chrome** (the earlier form of this decision) — rejected; the 2022 grays plus bevel edges read dated beside the rest of the product.
- **Token-only / std-widgets** — rejected; Application Bar and modal chrome need authored components.
- **Unity Editor Icon Library** — rejected this pass; recolor Godot SVGs instead.
- **Native File/Edit menus; Play tools on the Application Bar or a Scene Window Toolbar** — rejected; Save/Undo stay left, Play stays a centered cluster, viewport strips stay overlays.
- **Unity Numeric Field for TRS, Camera FOV, or other Inspector property rows; restyled Hierarchy row geometry, Hierarchy Line, or Add… information architecture** — rejected; those interiors stay the authored Godot-like design.
- **Freezing the Hierarchy selection colors (`#3a3a4a` / `#bde7ff`)** — rejected; one accent token owns selection everywhere, so Hierarchy selection follows it while row geometry and the square selection rectangle stay frozen.
- **Square (0px) corners** — rejected; keep Editor corner radius. Hierarchy selection rows stay square.
- **Near-black panel surfaces** (`#1B1E22` Window over `#14161A` App Bar, the first cut of this theme) — rejected on review as too dark; panels were raised to the Godot brightness anchor while the Application Bar stays the darkest layer.
- **Per-panel accent colors** — rejected; one accent token drives every accented state, with soft fill and line tints derived from it.
- **Teal / violet / amber / green accents** — rejected after side-by-side preview; amber also collides with dirty/warning meaning and weakens the Play affordance.

## Consequences

Static preview: `docs/previews/editor-theme-unity-dark.html` — Modern skin is the default; the earlier Unity Dark palette remains behind a toggle for comparison only, not as a shipped theme. A future reader who sees modern dark chrome next to Godot value cells and Godot glyphs should not "fix" them into numeric fields or a Unity icon set. Icon-versus-text on chrome actions is [ADR 0042](0042-icon-first-chrome-labels.md), not this palette decision. Glossary: `CONTEXT.md` (Editor chrome).
