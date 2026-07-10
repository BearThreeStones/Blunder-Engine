## Context

Dock chrome in `docking_panel.slint` uses `Text` with Unicode codepoints (`\u{2715}` close, `\u{1F4CC}` pin emoji, `\u{2013}` en dash). Slint's default embedded font covers Latin tab titles but not these symbols, so Windows renders tofu boxes. This is a display-only bug; `TouchArea` hit targets and C++ callbacks are correct.

The docking-auto-hide feature added more surfaces (strip unpin, overlay title bar) that reuse the same broken glyphs.

## Goals / Non-Goals

**Goals:**

- Font-independent vector icons for close, pin, and minimize on all dock chrome surfaces.
- Reusable Slint components with a `color` property for hover styling by parent touch areas.
- Visual parity with current dock chrome (size ~9–12 px, muted gray default, white on hover).
- Zero changes to dock layout model, C++ APIs, or callback wiring.

**Non-Goals:**

- Content Browser tree arrows, folder emoji, or other non-dock Unicode icons.
- Custom icon theme system or asset pipeline.
- Platform-specific `font-family` fallbacks.

## Decisions

### 1. New file `dock_chrome_icons.slint` with three export components

**Choice:** `DockChromeCloseIcon`, `DockChromePinIcon`, `DockChromeMinimizeIcon` in a dedicated Slint module imported by `docking_panel.slint`.

**Rationale:** Single source of truth; avoids duplicating geometry in tab, strip, and overlay layers.

**Alternatives considered:**

| Alternative | Why not |
|-------------|---------|
| ASCII `"x"` / `"-"` text | Works but inconsistent weight vs titles; still font-dependent |
| `font-family: "Segoe UI Symbol"` | Platform-specific; emoji pin still fails |
| Inline geometry in each `TouchArea` | Three copies of the same drawing logic |

### 2. Draw icons with rotated `Rectangle` bars (no `Path`)

**Choice:** Close = two 1 px bars at ±45° inside a 10×10 px box; minimize = one horizontal 1 px bar; pin = small filled head + vertical stem from rectangles.

**Rationale:** The codebase has no existing `Path` usage in Slint; thin rectangles with `transform rotation` (or fixed diagonal placement via `x`/`y` offsets) are proven Slint primitives and scale cleanly.

**Alternatives considered:**

- **Slint `Path`**: More flexible but unused in repo; higher review cost for marginal benefit on 3 tiny icons.
- **Embedded PNG/SVG**: Adds asset loading; overkill for 10 px chrome.

### 3. Parent owns hover color; icon is dumb drawable

**Choice:** Each component exposes `in property <color> color` (default `#808080`). Parent `TouchArea` sets `color: touch.has-hover ? #ffffff : #808080` (or existing per-surface muted colors).

**Rationale:** Matches current pattern where `Text.color` was bound to `pin-area.has-hover`. No internal hover state duplication.

### 4. Preserve existing touch geometry

**Choice:** Only replace inner `Text { ... }` with centered icon component; do not resize `TouchArea` rectangles (18×parent.height tab close, etc.).

**Rationale:** Recent auto-hide work tuned hit targets; changing sizes risks regressions.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Rotated rectangles look blurry at odd DPI | Keep bar width at 1 px; center in even-sized box; manual visual check at 100% and 125% scaling |
| Pin silhouette less recognizable than emoji | Use simple push-pin shape (circle head + stem); match common IDE affordance |
| Slint `rotation-origin` API quirks | Prototype close icon first in apply phase; fall back to pre-rotated offset math if rotation unsupported |

## Migration Plan

1. Add `dock_chrome_icons.slint` and wire into `docking_panel.slint` via `import`.
2. Replace five `Text` glyph sites (tab close, tab pin, strip unpin, overlay minimize, overlay close).
3. Build `engine_editor` Debug; visually verify Viewport tab ×, Hierarchy pin, auto-hide strip/overlay chrome.
4. Archive change; sync spec to `openspec/specs/dock-chrome-icons/spec.md`.

Rollback: revert Slint files only; no data migration.

## Open Questions

- None blocking — vector rectangle approach is sufficient for scoped dock chrome.
