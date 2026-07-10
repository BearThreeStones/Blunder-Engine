## Context

Blunder's three-layer drag model (`dock-guide-three-layers`) separates host edge dock, auto-hide edge zones, and container cross guides. Host four-edge guides render in `DockHostGuidesLayer` but still use square `DockGuideButton` (38×38, half-pane + dashed divider)—the same component as container cross guides. The ADS reference perimeter icon is visually distinct: white inward arrow, flat-wide body, blue chrome bar, full light-blue fill.

`DockAutoHideGuideButton` already implements this look for auto-hide perimeter icons, but `dock-host-edge-guides` spec explicitly forbids host guides from using that style. Path **B** extracts shared visual primitives so host and auto-hide layers share one pictograph implementation without sharing semantics or hit rules.

**Current files:**

| Area | File | Relevant symbols |
|------|------|------------------|
| Slint | `docking_panel.slint` | `DockGuideButton`, `DockAutoHideGuideButton`, `DockAutoHideGuideIcon`, `DockAutoHideGuideArrow`, `DockHostGuidesLayer` |
| C++ layout | `dock_guide_hit.cpp` | `computeHostGuideLayouts` (38×38 square) |
| C++ layout | `dock_auto_hide.cpp` | `computeAutoHideGuideLayouts` (arrow-inclusive rects) |
| Model | `dock_layout_model.h` | `DockHostGuideView` (slot, rect, highlighted, faint) |

## Goals / Non-Goals

**Goals:**

- Match reference image for host four-edge guides (arrow + flat-wide body, no dashed split)
- Extract **`DockEdgeGuideIcon`**, **`DockEdgeGuideArrow`**, **`DockEdgeGuideButton`** as shared visual building blocks
- Refactor `DockAutoHideGuideButton` to compose shared primitives (pixel-parity)
- Switch `DockHostGuidesLayer` to `DockEdgeGuideButton` with `faint` / `highlighted`
- Align C++ host guide layout + hit rects with arrow-inclusive bounds
- Consolidate dimension tokens into `DockEdgeGuideMetrics` (or alias existing globals)

**Non-Goals:**

- Changing container cross guide style (`DockGuideButton` unchanged)
- Changing host dock behavior (`dockToRoot`, half-window preview, proximity fade-in)
- Changing auto-hide drag semantics (edge zone only, no perimeter icons during drag)
- Re-enabling `DockAutoHideGuidesLayer` during tab drag
- Light-theme ADS clone

## Decisions

### D1: Path B — shared visual primitives, separate layer wrappers

**Decision:** Extract shared components; keep `DockHostGuidesLayer` and `DockAutoHideGuidesLayer` as thin wrappers with different data structs and visibility rules.

```text
DockEdgeGuideMetrics (shared tokens)
├── DockEdgeGuideArrow      ← rename/refactor from DockAutoHideGuideArrow
├── DockEdgeGuideIcon       ← rename/refactor from DockAutoHideGuideIcon + faint opacity
└── DockEdgeGuideButton     ← arrow + icon layout (edge-driven orientation)

Consumers:
  DockHostGuidesLayer       → DockEdgeGuideButton (slot→edge map in Slint)
  DockAutoHideGuideButton   → thin alias / inherits DockEdgeGuideButton (deprecated name kept for layer compat)
  DockAutoHideGuidesLayer   → unchanged wiring
```

**Alternatives:**

| Alternative | Why not |
|-------------|---------|
| Path A: host layer uses `DockAutoHideGuideButton` directly | Couples host semantics to auto-hide component name; harder to evolve independently |
| Path C: tweak `DockGuideButton` colors only | Cannot produce arrow + flat-wide + no dashed divider |

### D2: Metrics consolidation

**Decision:** Introduce `DockEdgeGuideMetrics` as the single source for arrow size, arrow gap, icon width/height, chrome height, body opacity. `DockAutoHideGuideMetrics` becomes an alias or re-exports the same properties for backward compat during refactor.

Keep frame/chrome/active colors in `DockGuideMetrics` (already shared by both guide families).

| Property | Default | Notes |
|----------|---------|-------|
| `arrow-size` | 8px | White `Path` triangle |
| `arrow-gap` | 3px | Between arrow and body |
| `icon-width` | 48px | Landscape width (top/bottom) |
| `icon-height` | 28px | Landscape height |
| `chrome-height` | 7px | ~25% of 28px body |
| `body-opacity` | 0.3 | Full-body light blue fill |
| `faint-opacity` | 0.45 | Whole component when faint |

C++ `DockLayoutMetrics` host guide layout SHALL use the same values as `auto_hide_guide_*` fields (already present in `dock_manager.h`).

### D3: Host guide layout — reuse auto-hide geometry

**Decision:** Replace `computeHostGuideLayouts(host, guide_size)` square layout with arrow-inclusive rects matching `computeAutoHideGuideLayouts`:

| Edge | Combined bounds | Body orientation |
|------|-----------------|------------------|
| Top / Bottom | 48 × 39 | Landscape 48×28, arrow above/below |
| Left / Right | 39 × 48 | Portrait 28×48, arrow beside |

Implementation options (pick one in apply):

1. **Shared helper** `computeEdgeGuideLayouts(host, metrics, out)` used by both host and auto-hide layout functions
2. **Delegate** `computeHostGuideLayouts` → call `computeAutoHideGuideLayouts` with same metric args, map `DockEdge` → `DockSlot`

Visibility threshold: reuse `host.width >= icon_w * 2` / `host.height >= icon_h * 2` guards from auto-hide layout.

### D4: Slot ↔ edge mapping

**Decision:** Map in Slint `DockHostGuidesLayer` (no C++ struct change required):

| `DockSlot` | `edge` prop |
|------------|-------------|
| left (1) | 0 |
| right (2) | 1 |
| top (3) | 2 |
| bottom (4) | 3 |

`DockHostGuideView` keeps `slot`; Slint converts when passing to `DockEdgeGuideButton`.

### D5: Hit testing uses full component rect

**Decision:** `hitHostGuideSlot` hit rects MUST match C++ layout rects (arrow + body). This supersedes the prior 38×38 square icon-only bounds.

Priority and three-layer model unchanged: host icon hit still beats auto-hide edge zone.

### D6: `DockEdgeGuideButton` faint support

**Decision:** Add `faint` property to shared button; `opacity: faint && !highlighted ? 0.45 : 1.0` on root (same rule as `DockGuideButton`). Apply to arrow + icon together.

Highlight: frame 1px→2px, stroke `#6ab0ff`; body opacity and arrow unchanged (existing auto-hide behavior).

### D7: `DockAutoHideGuideButton` refactor strategy

**Decision:** Replace body of `DockAutoHideGuideButton` with `DockEdgeGuideButton { edge: root.edge; highlighted: root.highlighted; }`. Keep `export component DockAutoHideGuideButton` as a deprecated-compatible alias so `DockAutoHideGuidesLayer` needs no struct changes.

Verify pixel parity via manual drag screenshot or Slint preview before/after.

## Risks / Trade-offs

- **[Risk] Visual confusion between host dock and auto-hide** — Both use same pictograph if auto-hide perimeter icons return later → Mitigation: host only visible near perimeter during drag; auto-hide layer currently hidden during drag; distinct preview overlays (half-window vs strip)
- **[Risk] Hit rect expansion steals auto-hide zone near icon** — By design (host icon priority); unchanged from three-layer model
- **[Risk] Metrics drift between C++ and Slint** — Mitigation: single `DockEdgeGuideMetrics` global; C++ reads existing `auto_hide_guide_*` fields; document sync in `dock_manager.h` comment
- **[Trade-off] Extra abstraction vs Path A** — Slightly more Slint components; cleaner long-term if perimeter icons reappear

## Migration Plan

1. Add shared Slint components + metrics (no consumer switch)
2. Refactor `DockAutoHideGuideButton` → compose shared primitives; verify visual parity
3. Switch `DockHostGuidesLayer` to `DockEdgeGuideButton`
4. Update C++ `computeHostGuideLayouts` / `hitHostGuideSlot`
5. Manual validation: drag tab near each host edge, confirm faint/highlight/preview/drop
6. Rollback: revert `DockHostGuidesLayer` to `DockGuideButton` + square layout (shared components can remain)

## Open Questions

- None blocking apply. Optional follow-up: delete `DockAutoHideGuideIcon` / `DockAutoHideGuideArrow` names entirely once alias period ends.
