## Context

Blunder's docking stack (`DockManager`, `DockDragController`, Slint overlays) already implements auto-hide **lifecycle** and drag-to-pin **detection/drop**. Split guides are positioned by `appendGuides` but rendered as flat rectangles in `DockGuidesLayer`. Auto-hide drag shows only a narrow strip preview—no perimeter guide buttons. Qt ADS reference uses pictographic grey buttons with cyan direction bars, a center tab-merge glyph, perimeter edge buttons with inward arrows, and simultaneous cross + edge guides during drag.

This extension completes the **visual and hit-test** layer on top of the landed two-layer drop model.

## Goals / Non-Goals

**Goals:**

- ADS-style **dark-theme** guide buttons for split cross (5) and auto-hide perimeter (4)
- Perimeter auto-hide guides **always visible** during drag when widget is pinnable (not only on hover)
- Dedicated **flat-wide** perimeter guide icons with white inward arrow **inside Slint** (not split half-pane icons)
- Expand auto-hide hit-test to **full-edge guide hit bands** plus preview-width outer zone; keep strip preview on hover
- Split cross and auto-hide guides visible **together** when dragging over a container (no mutual exclusion)
- Preserve landed detection, drop, tab insert, and split-dock behavior

**Non-Goals:**

- `open_on_drag_hover` (expand strip while hovering during drag) — follow-up change
- Pinning every widget inside a multi-tab float container in one gesture
- Changing auto-hide overlay title chrome
- Light-theme ADS clone (use dark editor palette)

## Decisions

### D1: Two-layer hit testing (outer auto-hide, inner split)

**Decision:** During drag, resolve drop target in order:

1. If pointer in **auto-hide guide hit band** (full edge span × guide depth) OR **host outer band** (preview-width, full edge span) → `DockEdge` auto-hide candidate (if pinnable + feature enabled)
2. Else if pointer over **hovered container content** → existing `computeSlotRaw` → split `DockSlot`
3. Else → no guide / float on release per current rules

**Outer band geometry:**

```text
m_host_rect edge
│← guide icon (visual, centered) ───────→│
│← guide hit band (full edge length) ────→│  ← hitAutoHideGuideEdge
│← outer zone = preview width (32px) ────→│  ← computeAutoHideDropEdge fallback
│←──────── inset_host / containers ────→│
         ↑ inner split cross guides
```

- Hit priority: guide hit band > outer preview-width zone > inner split
- **Outer zone depth** per edge: `max(auto_hide_drop_preview_size, strip_thickness)` when tabs exist on that edge; else `auto_hide_drop_preview_size` (32px). Spans **full** host height (left/right) or width (top/bottom).
- **Guide hit band** depth matches guide layout depth (39px left/right, 48px top/bottom); spans **full** edge length so dragging toward a centered guide icon still hits when pointer is above/below the icon.
- Intentionally wider than ADS `AutoHideAreaMouseZone` (8px) for usability; preview geometry and hit geometry stay aligned.

### D2: Visual differentiation (updated)

| Layer | Auto-hide perimeter | Split cross |
|-------|-------------------|-------------|
| **Visibility** | All 4 edge guides for entire pinnable drag | 5 guides when hovering a container (split hover active) |
| **Icons** | `DockAutoHideGuideButton` — **portrait** 28×48 (left/right) or **landscape** 48×28 (top/bottom) + white arrow | `DockGuideButton` — square half-pane + dashed divider (§7.8) |
| **Preview** | Narrow strip on **hovered** edge only | `previewRectForSlot` ~1/3 container |
| **Theme** | Frame `#4a90d9`; body `#4a90d9` @ ~30% opacity (full pane); arrow `#ffffff` | Frame + half-pane active; dashed divider |
| **Highlight** | **Bold blue frame** (2px → `#6ab0ff`); body/arrow unchanged | Brighter frame + stronger active fill |
| **Divider** | **None** | **Dashed** line at split boundary |

**Slint components:**

- `DockGuideButton` — split cross only (§7.8); slot 1–5 with dashed dividers
- `DockAutoHideGuideButton` — perimeter only (§7.9); `edge` prop; arrow + icon self-contained
- `DockGuideDashedHLine` / `DockGuideDashedVLine` — split cross only
- `DockGuideMetrics` — shared chrome/frame colors; add `auto-hide-body-opacity`, arrow size
- `DockGuidesLayer` — `DockGuideButton`
- `DockAutoHideGuidesLayer` — `DockAutoHideGuideButton` only (remove `DockGuideInwardArrow` sibling)
- `DockAutoHideDropPreviewLayer` — unchanged strip preview on hover

**C++:** `DockAutoHideGuideView` simplifies to `edge`, `rect` (combined arrow+icon bounds), `highlighted`. Remove `arrow_rect` / `show-arrow` from model and `DockAutoHideGuide` Slint struct.

### D3: Drag controller state

Unchanged from prior design: `hovered_auto_hide_edge`, `auto_hide_tab_insert_index`, split hover fields.

`handleDragMove`: auto-hide edge from guide rect hit OR outer band; split hover when not in auto-hide target and over container content.

### D4: Drop execution (`endDrag`)

Unchanged from prior design.

### D5: Pinnable gate

Perimeter guides render and auto-hide hit-test **only** when pinnable + `feature_enabled`. Non-pinnable drags show split cross only (no perimeter guides).

### D6: Metrics constants

Existing fields plus:

| Field | Default | Use |
|-------|---------|-----|
| `guide_size` | 38px | Split cross button square (existing) |
| `guide_gap` | 6px | Cross spacing (existing) |
| `auto_hide_drop_mouse_zone` | 8px | Legacy ADS floor; effective outer depth uses preview size (§7.17) |
| `auto_hide_drop_preview_size` | 32px | Strip preview depth **and** default outer hit depth (empty edge) |
| `auto_hide_guide_width` | 48px | Landscape icon width (top/bottom); portrait icon **height** (left/right) |
| `auto_hide_guide_height` | 28px | Landscape icon height (top/bottom); portrait icon **width** (left/right) |
| `auto_hide_guide_arrow_size` | 8px | White triangle arrow (Slint `Path`) |
| `auto_hide_guide_arrow_gap` | 3px | Gap between arrow and window icon inside component |

Perimeter guide position: centered on each host edge; `rect` is the full Slint component bounds (arrow on outer side + oriented icon toward interior).

### D7: Split preview factor

Unchanged: ~1/3 container for split previews.

### D8: Remove guide mutual exclusion

**Decision:** Delete `editor_window.slint` condition hiding `DockGuidesLayer` when `docking-auto-hide-drop-preview.visible`. Both layers render independently during drag.

**Rationale:** Matches Qt ADS reference screenshot—cross on container and perimeter buttons coexist.

### D9: Mini-window cross guide icons with dashed dividers (§7.8)

**Decision:** Replace abstract single-edge cyan bars with ADS **mini-window** pictographs inside each `DockGuideButton`:

```text
┌─ blue frame ────────┐
│ ███ chrome bar ███  │  ← solid #4a90d9, ~9px
│ ░░ active half ░░░  │  ← semi-transparent active zone (slot-dependent)
│ ┄ ┄ ┄ ┄ ┄ ┄ ┄ ┄     │  ← dashed divider (required; not solid line)
│    inactive half    │  ← #2a2a2a
└─────────────────────┘
```

- **Top/Bottom slots:** horizontal dashed divider at content midline
- **Left/Right slots:** vertical dashed divider at content midline
- **Center slot:** full content active fill, no divider (tab merge)
- **Dashed rendering:** `DockGuideDashedHLine` / `DockGuideDashedVLine` repeat 3×1px (or 1×3px) segment rects; tunable via `DockGuideMetrics`
- **No Slint dash stroke:** native `Path` stroke-dash not available; segment workaround is mandatory

**Alternatives considered:**

| Alternative | Why not |
|-------------|---------|
| Solid 1px divider | Rejected — user requires dashed fidelity to ADS reference |
| `Path` with dash array | Not supported in current Slint fork |
| Separate cross-only component | Rejected for split — `DockGuideButton` stays cross-only |

### D10: Perimeter auto-hide guide icons (§7.9)

**Decision:** Perimeter guides use a **separate** `DockAutoHideGuideButton`, visually distinct from split cross `DockGuideButton` (§7.8 unchanged).

```text
TOP edge (outer side up):
        ▼  white Path triangle
   ┌──────────────┐
   │ ███ chrome   │  ← solid #4a90d9 bar
   │░░ full body ░│  ← entire content = light blue (~30% opacity), NO dashed line
   └──────────────┘
        host interior ↓
```

- **Aspect ratio:** **landscape 48×28** on top/bottom; **portrait 28×48** on left/right (icon taller than wide, matching vertical auto-hide strips)
- **Arrow:** white filled triangle via Slint `Path`, drawn **inside** component on the outer edge (top/bottom/left/right); remove `DockGuideInwardArrow` and C++ `arrow_rect`
- **Highlight:** **bold blue frame only** (`border-width` 1px → 2px, stroke `#6ab0ff`); body opacity and arrow color unchanged
- **No half-pane / no dashed divider** — semantic: pin whole widget to edge strip, not split container

**Layout inside component (top edge example):**

```text
┌─ root rect (48×28 total includes arrow) ─┐
│  [arrow 8px] [gap 3px] [window icon rest] │
└──────────────────────────────────────────┘
```

Left/right edges: arrow on outer left/right; **portrait** window icon (28×48) beside arrow. Top/bottom: arrow above/below **landscape** icon (48×28). Hit-test uses root `rect` from C++.

### D11: Portrait icons on left/right perimeter guides (§7.14)

**Decision:** Perimeter mini-window **orientation follows host edge**:

| Edge | Icon orientation | Icon size | Combined bounds (incl. arrow) |
|------|----------------|-----------|--------------------------------|
| Top / Bottom | Landscape (wide) | 48×28 | 48 × (8+3+28) = 48×39 |
| Left / Right | **Portrait (tall)** | **28×48** | (8+3+28) × 48 = **39×48** |

```text
LEFT edge:                    TOP edge:
  ◀ ▼ arrow                      ▼ arrow
 ┌──┐                      ┌────────────┐
 │██│ chrome (top)         │██ chrome   │
 │░░│                       │░░ body    ░│
 │░░│ portrait body         └────────────┘
 │░░│                       landscape
 └──┘
```

- **Rationale:** Vertical auto-hide strips run along left/right; portrait icons read as “dock into side strip” rather than a horizontal window
- **Slint:** `DockAutoHideGuideButton` sets icon aspect from `edge` (0/1 portrait, 2/3 landscape); chrome bar remains at top of icon rect in both orientations
- **C++:** `computeAutoHideGuideLayouts` swaps `auto_hide_guide_width` / `auto_hide_guide_height` for left/right icon dimensions; visibility threshold uses portrait height (48px) on vertical edges

### D12: Widen auto-hide drag hit targets (§7.17)

**Decision:** Address narrow drag-to-pin feel on **both** empty edges and centered guide icons:

| Problem | Cause today | Fix |
|---------|-------------|-----|
| Empty edge hard to hit | 8px outer band | Outer band depth = **`auto_hide_drop_preview_size` (32px)**, full edge span |
| Guide icon miss (above/below icon) | Guide hit = small centered rect only | **Guide hit band** = full host height (L/R) or width (T/B) × guide depth |
| Strip already visible | Only 28px when lucky | `max(preview_size, strip_thickness)` per edge (unchanged rule, larger base) |

```text
LEFT edge — hit regions (side view):

y │████████████████│  ← guide hit: full height × 39px deep
  │████ icon ██████│  ← visual guide stays centered (Slint unchanged)
  │████████████████│  ← outer zone: full height × 32px (same span, may overlap)
  └────────────────┘→ x
  0        32   39    inner host
```

- **Slint:** visual `DockAutoHideGuide` rects unchanged (centered icons)
- **C++:** `hitAutoHideGuideEdge` uses expanded `hit_rect` per edge; `computeAutoHideDropEdge` takes `preview_size` as default zone depth instead of `mouse_zone` (8px)
- **Trade-off:** Outer 32px band reduces split-dock area near host edges; acceptable—auto-hide is intentionally outer-priority

### D13: Global pointer routing for in-host tab drag (§7.20)

**Problem:** Widened C++ hit bands (§7.17) did not fix user reports that auto-hide only triggers at spotty locations (top-left corner, right-center, etc.). Root cause is **input routing**, not geometry:

```text
Tab TouchArea (small)          Host outer edge
┌──[Hierarchy tab]──┐          │
│  moved events     │  ──drag──►│  no tab-moved once pointer leaves tab
│  only while here  │          │  → last position stuck near tab origin
└───────────────────┘          └──────────────────►
```

Slint `DockTabsLayer` `moved` fires only while the pointer is over the tab strip. Once the user drags toward a host edge, `tab-moved` stops and `handleDragMove` never sees edge coordinates.

**Decision:** Reuse existing `SlintSystem::tickGlobalDockPointerPoll` (SDL global mouse → dock-local) for **in-host** tab drags after `exceededDragThreshold()`, not only native OS floats.

| Drag origin | Pointer source after threshold | Coordinate path |
|-------------|-------------------------------|-----------------|
| Native OS float | Poll every frame (unchanged) | `screenToDockLocal` |
| In-host docked tab | Poll every frame (**new**) | `pointerToDockLocal` |
| In-host before threshold | Slint `tab-moved` only | tab-local coords |

**Slint callback gating:** When `dragNeedsGlobalPointerPoll()` is true, ignore `docking-tab-moved` / `docking-tab-released` from `DockChromeBar` so poll is the single source of truth (avoids duplicate `handleDragMove` / double `endDrag`).

**`dragNeedsGlobalPointerPoll()` logic:**

```cpp
if (!m_drag.isActive()) return false;
if (nativeFloatingNodeForActiveDrag()) return true;  // unchanged
return m_drag.exceededDragThreshold();               // in-host after 8px
```

**Non-goals:** Full-window Slint capture overlay (deferred unless poll proves insufficient on a platform).

## Risks / Trade-offs

- **[Risk] Visual clutter with 4 perimeter + 5 cross guides** → Acceptable for ADS parity; only during active drag
- **[Risk] Hit-test expansion to guide rects may steal split hovers near host edges** → Outer auto-hide takes priority by design (ADS-aligned)
- **[Risk] Float drag + auto-hide drop leaves orphan float** → Existing teardown path unchanged
- **[Trade-off] Arrows add layout complexity on small hosts** → Clamp arrow size; hide arrows if host dimension < 2× guide_size

## Migration Plan

1. Land `DockGuideButton` + restyle split layer (visual-only, behavior unchanged)
2. Add perimeter guides + model sync + hit-test expansion
3. Remove mutual exclusion; manual validation cross + edge guides together
4. Rollback: hide `DockAutoHideGuidesLayer`; revert to flat rectangles

## Open Questions

- Resolved: **dark theme** for guides (not light ADS clone)
- Resolved: perimeter guides **always visible** during pinnable drag
- Resolved: **inward arrows** on perimeter guides in v1
- Resolved: split cross icons use **mini-window + dashed divider** (§7.8), not single-edge bars
- Resolved: perimeter icons are **flat-wide full-pane** with embedded white arrow (§7.9); split cross unchanged
- Resolved: perimeter **highlight = bold blue frame** only
- Resolved: arrow **inside Slint**; drop C++ `arrow_rect` model fields
- Resolved: left/right perimeter icons are **portrait (28×48)**; top/bottom remain **landscape (48×28)** (§7.14)
- Resolved: outer auto-hide hit depth = **preview width (32px)** + guide **full-edge** hit bands (§7.17)
- Resolved: in-host drag uses **global pointer poll** after threshold; tab `moved` alone is insufficient (§7.20)
