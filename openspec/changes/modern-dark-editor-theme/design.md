## Context

See proposal.md for motivation. ADR 0039 and `CONTEXT.md` (Editor chrome) are already grilled. Visual contract: `docs/previews/editor-theme-unity-dark.html` (Modern skin default). Today there is no theme object: `editor_window.slint` uses `#181818` and left-aligned `std-widgets` Buttons; Hierarchy hardcodes `#3a3a4a` / `#bde7ff`; Inspector / Browser / PM / dialogs mix hex and `std-widgets`. `DockChromeMetrics.bar-height` (32px) is kept in sync with C++ `DockLayoutMetrics::chrome_bar_height`.

## Goals / Non-Goals

**Goals:**
- One Slint-exported theme global as the color/metric source of truth
- A small Editor controls component kit that replaces `std-widgets` Button/LineEdit/CheckBox/Slider in editor chrome
- Sequenced restyle so tokens land before any panel is rewritten
- Keep dock hit-testing metrics stable unless a measured tab-well change is required

**Non-Goals:**
- A C++ theme runtime or USS/UI Toolkit port
- Light theme, Unity icon PNGs, native menus
- Changing Document History, Add… IA, Hierarchy Line geometry, or Vulkan overlays
- Shipping the HTML preview as product UI

## Decisions

### D1 — Slint global is the token table
**Choice:** New `editor_theme.slint` exports a `global EditorTheme` (colors, radii, line heights, derived accent-soft / accent-line / accent-hover). All editor windows import it. No parallel C++ palette this pass.
**Why:** Every consumer is Slint; hex-as-source-of-truth is the bug. Derived tints (soft ~17%, line ~45%) stay in one place.
**Rejected:** C++ struct mirrored into Slint properties (sync tax); per-file `:root` copies of the preview CSS.

### D2 — Sequenced restyle, one change
**Choice:** Apply in this order: (1) tokens + window backgrounds, (2) Application Bar + dock frames/tabs, (3) Editor controls primitives, (4) authored modals, (5) Content Browser / History / Viewport strips / PM, (6) Inspector Foldout / Add… / Remove, (7) Hierarchy selection color + Inspector property-field fill only.
**Why:** Grill required sequenced tasks; primitives must exist before panels swap `std-widgets`.
**Rejected:** Big-bang rewrite of every `.slint` in one commit; a second OpenSpec change per layer.

### D3 — Controls are authored Slint, not std-widgets restyle
**Choice:** `editor_controls.slint` (or split files) provides Button, ToolbarButton, Tab, Foldout, TextField, SearchField, Toggle, Slider, ColorField, ObjectField, and a modal chrome wrapper. Inventory matches Unity Foundations; look is Editor Theme (hairline, 6px radius, 22px single-line, accent primary/checked/focus).
**Why:** `std-widgets` cannot express ghost App Bar buttons, pill tabs, or accent primaries without fighting the Fluent/Material skins.
**Rejected:** Theming std-widgets via palette injection; wrapping every Button in a one-off Rectangle.

### D4 — Dock chrome height stays 32px unless tabs clip
**Choice:** Keep `DockChromeMetrics.bar-height` / C++ `chrome_bar_height` at 32px. Pill tabs and 10px window radius live *inside* that bar. Application Bar (~48px) is a separate strip above the dock host and does not change dock hit-testing.
**Why:** Dock hit-test, float chrome, and auto-hide overlays all key off that height.
**Rejected:** Bumping chrome to 34px in the first sequence step. Revisit only if pill tabs clip at 32px.

### D5 — Accent selection is a derived fill, not a second palette
**Choice:** Hierarchy selected row, List/Tree selection, PM selected project, and Browser thumbnail outline all use `EditorTheme` accent-soft / accent-line / accent-tinted text. Soft fill (~22% over Window) for dense 22px rows; outline for thumbnails.
**Why:** One token owns selection. Solid accent fill on Hierarchy rows is too loud at 22px (preview choice).
**Rejected:** Keeping `#3a3a4a` / `#bde7ff` on Hierarchy; a per-panel highlight hex.

### D6 — Inspector leftover `#bde7ff` is chrome, not freeze
**Choice:** Inspector *section* chrome (Add… label, Absolute/Delta, enabled links) migrates to Editor controls / accent. Inspector *property fields* stay AxisNumberField / compact cells with `EditorTheme` recessed fill (`#1E2125`), not Numeric Field. Authored Inspector row-highlight behavior (if any) is not restyled into Unity highlight.
**Why:** Matches Inspector control skin vs Inspector property field. `#bde7ff` is not a frozen Hierarchy color.
**Rejected:** Replacing FOV/TRS cells with Editor controls Numeric Field; leaving Add… as `std-widgets` Button.

### D7 — Modal dim blur is best-effort in Slint
**Choice:** Editor modal chrome: 14px radius, no titlebar divider, accent primary bottom-right, dim overlay. Use Slint `DropShadow` / backdrop where the fork supports it; if backdrop-filter is missing, keep a 58% dim without blur rather than adding a Vulkan blur pass.
**Why:** Preview used CSS `backdrop-filter`; the Slint fork may not. Spec requires dim + radius + primary, not a GPU blur.
**Rejected:** A new offscreen blur; OS DisplayDialog for these flows.

### D8 — Icon recolor stays Image.colorize
**Choice:** Existing Godot SVG Slint components keep `Image.colorize` with `EditorTheme.icon` (`#B3BBC4`); checked ToolbarButtons colorize with accent.
**Why:** `godot-editor-icons` already landed this pattern.
**Rejected:** Unity Editor Icon Library this pass.

## Risks / Trade-offs

- [Dock tab pills clip at 32px] → Mitigation: D4; measure in task 2 before changing C++ metrics
- [std-widgets leftovers in a late panel] → Mitigation: sequenced tasks; grep `std-widgets` as a done check for editor chrome files (animation tree canvas may keep local chrome if not in this Shell pass — treat as follow-up if still mixed)
- [Slint DropShadow / clip + radius bugs on dock frames] → Mitigation: radius on the frame Rectangle with `clip: true`; fall back to hairline-only if clip fights splitters
- [Hierarchy tests assert `#3a3a4a`] → Mitigation: update color assertions; geometry tests stay
- [Preview HTML drifts from Slint] → Mitigation: preview is the visual contract for this change; after ship, do not treat HTML as runtime

## Migration Plan

1. Land `editor_theme.slint` + wire window backgrounds; product still looks old except fills
2. App Bar + dock frames/tabs
3. Editor controls kit; swap `std-widgets` in App Bar and modals
4. Browser, History, Viewport strips, PM
5. Inspector Foldout/Add…/Remove; property-field fill; Hierarchy accent selection
6. Build `engine_editor` + `project_manager`; grep remaining chrome hex / std-widgets
7. Rollback: revert Slint files; no data-format change

## Open Questions

None — grilled into ADR 0039 / CONTEXT.md / the HTML preview.
