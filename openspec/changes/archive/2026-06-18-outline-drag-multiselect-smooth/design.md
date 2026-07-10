## Context

`outline-selected` shipped `OutlineOverlay` with:

- Surface prepass → `R16_UINT` object ID (constant `1`) + `D32` outline depth.
- Resolve: 4-neighbor hard edge, fixed `#F57011`, depth occlusion `alpha_occlu = 0.35`.
- Single `EditorSelectionSystem::getSelection()`; subtree mesh collection for parent picks.

Blender's overlay outline (`DRW_pass_outline_prepass` / `overlay_outline_detect_frag.glsl`) packs `color_id` and `ob_id`, supports multiple selected bases, switches color while transforming, and smooths overlay edges when smooth-wire is enabled.

`TransformGizmoController::isDragging()` already exists and is used by gizmo rendering; outline does not read it yet.

## Goals / Non-Goals

**Goals:**

- Distinct outline color while transform gizmo is dragging (object-select vs transform orange).
- Smooth (anti-aliased) outline edges by default in resolve—no new UI toggle in this change.
- Multi-entity selection in the editor with correct combined outlining (no seams between co-selected meshes).
- Per-object `ob_id` in prepass with Blender-style edge rules: silhouette vs background; suppress edges between same-`color_id` neighbors.
- Preserve v1 pipeline insertion (after forward, before overlay lines) and subtree mesh expansion per selected root.

**Non-Goals:**

- X-Ray edge-detection geometry (`DRW_cache_mesh_edge_detection_get`).
- Thick outline expansion (HiDPI width > 2 px)—separate follow-up.
- `V3D_SELECT_OUTLINE` UI preference toggle.
- Theme system / user-configurable colors beyond the two hardcoded oranges.
- GPU picking / selection buffer pass.
- Viewport click multi-select (Hierarchy + keyboard modifiers only for v1).
- Per-object *different* outline colors in multi-select (all selected share object-select orange; IDs exist for edge rules and future styling).

## Decisions

### 1. ID packing in `R16_UINT`

**Decision:** Pack as Blender does for v1 scope:

```
packed = (color_id << 14) | (ob_id & 0x3FFF)
```

- `color_id`: 2 bits — `0` = transform drag, `1` = object select (v1 uses only 0 and 1).
- `ob_id`: 14 bits — unique per selected **root entity** (not per primitive); all subtree mesh draws for that root share one `ob_id`.

**Rationale:** Fits `R16_UINT`; resolve can compare full word for edge detection and mask `color_id` for color lookup.

**Alternatives considered:**

- Separate color attachment — rejected (bandwidth + barrier complexity).
- Single constant ID for all multi-select — rejected; causes internal seams or wrong edge logic when objects touch.

### 2. Resolve edge rules (multi-select)

**Decision:** Fragment is an edge when center `packed != 0` AND any 4-neighbor differs AND:

- Neighbor is `0` (background silhouette), **OR**
- `(neighbor >> 14) != (center >> 14)` (`color_id` change).

Do **not** draw an edge when neighbors differ only in `ob_id` but share the same `color_id` (co-selected objects touching).

**Rationale:** Matches Blender multi-select outline: one outline hull per selection group color, no lines between co-selected meshes.

### 3. Transform-drag color

**Decision:** In `OutlineOverlay::begin_sync`, read `TransformGizmoController::isDragging()`. Pass `color_id = 0` to prepass for all selected mesh draws while dragging; `color_id = 1` otherwise. Resolve maps:

| color_id | Color (linear RGB) | Notes |
|----------|-------------------|--------|
| 0 | `(0.96, 0.44, 0.07)` transform | Blender `theme.colors.transform` ≈ same family; tune to slightly brighter orange if needed |
| 1 | `(0.960784, 0.439216, 0.066667)` | Existing `#F57011` object select |

**Rationale:** Minimal change—color driven by packed ID high bits, no separate color buffer.

### 4. Smooth outline resolve

**Decision:** Replace binary 1-neighbor edge with a **smooth silhouette mask** in `outline_resolve.slang`:

1. Sample 4 cardinal neighbors (keep existing depth occlusion path).
2. Compute `coverage = max over neighbors of smoothstep` based on whether neighbor ID is "foreground" (same edge rules as hard edge).
3. Multiply alpha by `coverage` (and existing `kMaxOutlineAlpha` cap).

Use a small filter (cardinal + optional 4 diagonal samples for 8-neighbor) with `smoothstep(0.0, 1.0, …)` on ID discontinuity—no separate `overlay_aa` pass for outline in v1.

**Rationale:** Aligns with Blender smooth overlay wire intent without adding MRT. Grid/line overlays keep their own AA path.

**Alternatives considered:**

- Route outline through `OverlayAntiAliasing` — rejected; outline is direct color composite, not line MRT.
- MSAA on prepass — rejected; heavier and inconsistent with offscreen scale.

### 5. Editor multi-selection API

**Decision:** Extend `EditorSelectionSystem`:

```cpp
void setSelection(EntityId id);           // replaces selection (single)
void addToSelection(EntityId id);         // add if valid, no-op if duplicate
void removeFromSelection(EntityId id);
void toggleSelection(EntityId id);
void clearSelection();
eastl::vector<EntityId> getSelectedIds() const;
EntityId getPrimarySelection() const;     // last selected; inspector focus
bool isSelected(EntityId id) const;
```

Keep `getSelection()` as alias for `getPrimarySelection()` for backward compatibility.

**Hierarchy input (v1):**

- Click: `setSelection`
- Shift+click: `addToSelection`
- Ctrl+click: `toggleSelection`

**Rationale:** Standard DCC pattern; primary drives inspector/gizmo pivot (unchanged for v1).

### 6. Outline prepass batching

**Decision:** For each ID in `getSelectedIds()`, run existing subtree mesh collection; assign monotonically increasing `ob_id` per root (1..N). All draws in one prepass render pass (single clear).

**Rationale:** Reuses `outline-selected` subtree fix; one prepass per frame.

### 7. `ob_id` limit

**Decision:** Cap at 4094 simultaneous selected roots (`0x3FFF` reserved for background). If exceeded, log once and skip outline for overflow entries.

**Rationale:** 14-bit field limit; far beyond practical editor use.

## Risks / Trade-offs

- **[Risk] Hierarchy modifier keys not plumbed through Slint** → Read SDL modifier state in `on_hierarchy_entity_selected` callback or Win32 poll path; document in tasks.
- **[Risk] Inspector/gizmo only know primary selection** → Accept for v1; multi-select is outline-first.
- **[Risk] Smooth resolve softens 1px outline too much at low res** → Tune smoothstep width; test at `BLUNDER_EDITOR_RENDER_SCALE`.
- **[Risk] Touching co-selected objects with different `ob_id` but same `color_id`** → Edge suppression rule handles; verify with Sponza multi-primitive under one parent vs two separate roots.
- **[Risk] Packed ID compare in shader** → Add unit-style shader comments; validate with RenderDoc ID buffer.

## Migration Plan

- Additive shader and selection API changes; no asset migration.
- Default smooth outline on immediately (no env gate).
- Rollback: revert resolve shader to hard edge + constant ID `1`; selection API can keep multi-select unused.

## Open Questions

- Exact transform orange RGB vs Blender theme—tune visually during `/opsx:apply` (start with object-select hue shifted +10% brightness).
- Whether viewport 3D pick should participate in multi-select — deferred; Hierarchy-only for this change.
