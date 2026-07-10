## Context

`dock-auto-hide-drag-drop` added perimeter `DockAutoHideGuideButton` icons tied to auto-hide pin, full-edge hit bands, and always-on visibility during pinnable drags. User testing showed edge detection only works where `tab-moved` still fires (top tab row / corners), not along full window edges. Exploration with the Qt ADS reference screenshot clarified the target: **three independent layers** with no overlapping semantics.

**User decisions (locked):**

| # | Choice |
|---|--------|
| 1 | **B** — All four edges have host dock guide icons; auto-hide on all four edges via thin edge band (independent of icons) |
| 2 | All four host guides **fade in near perimeter**; **highlight** only the icon under cursor |
| 3 | Container **cross guides** also **hover-icon-only** (faint when eligible, highlight on cursor) |
| 4 | New change `dock-guide-three-layers` (does not extend `dock-auto-hide-drag-drop` artifacts) |

## Goals / Non-Goals

**Goals:**

- ADS-aligned **three-layer** drag-drop: host edge dock | auto-hide edge zone | container cross
- Host edge drop → `dockToRoot(widget, slot)` with **half-window** preview (not 1/3, not narrow strip)
- Auto-hide drop → `setWidgetAutoHide` via **edge zone only** on all four edges when pinnable
- Guide **visibility**: proximity fade-in for host guides; hover highlight for host + cross icons
- **Outer margin** excludes container cross near host perimeter
- Reliable pointer updates for full-edge auto-hide (poll or Slint capture)

**Non-Goals:**

- Changing auto-hide lifecycle (expand/collapse/unpin/hover timers) beyond drag-to-pin detection
- `open_on_drag_hover` during drag
- Light-theme ADS clone
- Replacing split cross mini-window visual style (§7.8 from prior change)

## Decisions

### D1: Three-layer hit priority

During active tab drag, resolve target in strict order:

```text
1. Host edge guide icon hit (icon rect only)
      → hovered_host_slot, half-window preview, dockToRoot on drop
      → suppress auto-hide + cross for this frame

2. Auto-hide edge zone (thin band on all four edges, pinnable + feature_enabled)
      → hovered_auto_hide_edge, strip preview, setWidgetAutoHide on drop
      → suppress cross for this frame

3. Container interior (content rect minus chrome, outside outer margin)
      → hitTest container → cross icon hit OR slot from icon hover
      → dockWidget(container, slot) on drop

4. Else → float / no dock on release
```

```text
m_host_rect
│← edge zone (auto-hide) ─→│← outer margin ─→│
│  [host guide icon]        │  (no cross)     │  container + cross
│  center of edge           │                 │
```

### D2: Host edge dock guides

| Property | Value |
|----------|--------|
| Position | Centered on each host edge (reuse `computeAutoHideGuideLayouts` geometry or dedicated `computeHostGuideLayouts`) |
| Visual | `DockGuideButton`-style mini-window icon (same family as split cross); **not** auto-hide flat-wide + arrow |
| Hit | **Icon bounds only** — no full-edge band |
| Preview | `previewRectForHostSlot(m_host_rect, slot)` → **50%** of host width or height |
| Drop | `dockToRoot(widget, slot)` |
| Visibility | During drag: emit all four guides when pointer within `host_guide_proximity` of host edge; `highlighted` only on icon under cursor |
| Strip preview | Large semi-transparent overlay on hovered edge (half host), distinct from auto-hide strip |

**All four edges** including left (choice B): left host dock and left auto-hide can coexist; host icon hit takes priority over edge zone when both overlap at icon location.

### D3: Auto-hide drag (decoupled)

| Property | Value |
|----------|--------|
| Trigger | `computeAutoHideDropEdge` only — **remove** `hitAutoHideGuideEdge` from `handleDragMove` |
| Edges | All four (left, right, top, bottom) |
| Depth | `max(auto_hide_drop_preview_size, strip_thickness)` per edge when strip exists; else `preview_size` (32px default) |
| Preview | Narrow strip (`DockAutoHideDropPreviewLayer`) — unchanged |
| Icons | **No** auto-hide perimeter icons during drag (or optional static strip tabs only); pin does not require hovering an icon |
| Pinnable gate | Unchanged |

### D4: Container cross guides (hover-icon-only)

| Property | Value |
|----------|--------|
| Eligibility | Pointer over container **content** rect, outside `outer_margin`, not in host-icon or auto-hide zone |
| Visibility | Emit five cross guides at container center when eligible; **all faint** (`opacity` / inactive stroke) |
| Highlight | Only guide whose icon rect contains pointer |
| Hit | **Icon rect only** for slot selection (not `computeSlotRaw` from pointer position in content) |
| Preview | `previewRectForSlot(content_rect, slot)` — **1/3** container (unchanged) |
| Drop | `dockWidget(hovered_node_id, hovered_slot, widget)` |

**Migration from `computeSlotRaw`:** Slot derives from which cross icon is hovered, not nearest-edge fraction. Stabilizer may apply to icon hover changes only.

### D5: Outer margin

- New metric: `dock_outer_margin` (default **24px**, tunable; ADS uses similar outer distance).
- Container cross and `computeSlotRaw` **disabled** when `pointer` is within `outer_margin` of any host edge (inside edge zone is handled by layers 1–2 first anyway).
- Layout tiles unchanged; margin is hit-test only.

### D6: Guide proximity fade-in (host)

- New metric: `host_guide_proximity` (default **48px**): when pointer is within this distance of any host edge during drag, emit all four host guide icons at reduced opacity.
- Icon under cursor: full opacity + bold frame + half-window preview visible.
- Slint: `DockHostGuideButton` with `proximity-visible`, `highlighted`, `faint-opacity` props.

### D7: Cross proximity (optional faint)

- When pointer inside container content (outside outer margin), show all five cross icons faint immediately (no extra proximity band — container interior is the proximity region).
- Highlighted icon under cursor only.

### D8: Pointer routing (prerequisite)

- **Must** have continuous dock-local pointer during in-host tab drag (implement or land `dock-auto-hide-drag-drop` §7.20: global poll after threshold, or `DockDragCaptureLayer` on `docking_host`).
- Without this, auto-hide edge zones along full left/right height remain broken regardless of hit geometry.

### D9: Slint layer structure

```text
docking_host (top → bottom)
  … tiles, floatings …
  DockHostGuidesLayer          ← NEW: half-window preview + 4 host icons
  DockAutoHideDropPreviewLayer ← narrow strip (auto-hide only)
  DockGuidesLayer              ← cross icons (faint + hover highlight)
  DockChromeBar                ← tabs
  DockDragCaptureLayer         ← optional: full-host pointer capture when drag active
```

Remove or repurpose `DockAutoHideGuidesLayer` for drag (no perimeter auto-hide icons). Auto-hide strip tabs layer unchanged.

### D10: Drag controller state

Extend `DockDragController`:

| Field | Use |
|-------|-----|
| `hovered_host_slot` | `DockSlot` for host edge dock |
| `hovering_host_guide` | bool |
| `hovered_auto_hide_edge` | existing |
| `hovered_node_id` / `hovered_slot` | cross icon hover only |

`markDropped()` branches: host guide → `dockToRoot`; auto-hide → `setWidgetAutoHide`; cross → `dockWidget`.

## Risks / Trade-offs

- **[Risk] Left edge: host dock vs auto-hide confusion** → Host icon hit wins; edge zone is fallback when not on icon; distinct previews (half vs strip).
- **[Risk] Cross hover-only slower for power users** → Acceptable for ADS parity; icon targets are 38px.
- **[Risk] Pointer routing still broken** → Block implementation on D8; verify with left-edge mid-height test (§7.23 analogue).
- **[Risk] Breaking change vs `dock-auto-hide-drag-drop`** → Document supersession; reuse Slint components where possible.
- **[Trade-off] Half-window host dock is aggressive** → Matches ADS reference; user explicitly requested.

## Migration Plan

1. Land pointer routing (§7.20 or capture layer) if not already in binary.
2. Add host guide model + layer; wire half preview + `dockToRoot` drop.
3. Remove auto-hide guide icon hit path; keep edge zone only.
4. Rework cross to icon-hover slots; add outer margin.
5. Remove always-visible auto-hide perimeter guides during drag.
6. Manual QA: ADS screenshot parity checklist.
7. Archive `dock-auto-hide-drag-drop` after this change lands (or mark overlapping tasks cancelled).

## Open Questions

- Resolved: four host edges (B), proximity fade + hover highlight, cross hover-only, new change.
- Resolved: host preview = half window.
- Deferred: exact `host_guide_proximity` and `dock_outer_margin` pixel values (defaults above; tune in QA).
