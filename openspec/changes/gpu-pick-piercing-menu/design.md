## Context

`viewport-mesh-pick` implemented dual-path picking: CPU raycast (default) + GPU ID prepass (optional via env or triangle budget). Code lives in `ViewportPickSystem`, `PickOverlay`, and `RenderSystem::onEvent`. Unity's [Selection Piercing Menu](https://docs.unity3d.com/Manual/SelectionPiercingMenu.html) addresses overlapping objects at the same screen pixel via **Ctrl+right-click** popup and optional **repeat left-click cycling**.

GPU pick currently reads only the **front-most** 1×1 pixel sample—insufficient for piercing menu multi-hit lists.

**Locked for this change:**

- GPU pick is the **only** viewport pick path.
- Left-click modifiers unchanged: replace / Shift add / Ctrl toggle.
- Piercing menu: **Ctrl+right-click** replace, **Ctrl+Shift+right-click** add.
- Blend-transparent meshes remain non-pickable; alpha-cutout parity preserved in pick shader.

## Goals / Non-Goals

**Goals:**

- Remove CPU pick path and dual-path routing entirely.
- GPU front-most pick for all left-click selection.
- GPU **depth-peel** query returning ordered distinct `EntityId` hits at a pixel for piercing menu + click cycling.
- Slint popup piercing menu at cursor with entity names.
- Document updated input priority in `render-pipeline.md`.

**Non-Goals:**

- Blender-style pie menu or general viewport context menu (piercing only).
- Picking through locked/hidden layers (no layer system yet).
- Parent-entity promotion in piercing list (leaf entities only).
- BVH or CPU fallback if GPU peel fails.
- Changing Hierarchy modifier semantics.

## Decisions

### 1. Delete CPU pick; GPU always on

**Decision:** Remove `pickCpu`, `countPickableTriangles`, `shouldUseGpuPick`, `BLUNDER_VIEWPORT_GPU_PICK`, and triangle-count cache from `ViewportPickSystem`. Left-click always calls `RenderSystem::pickEntityAtWindowPosition`.

**Rationale:** User request; reduces maintenance. `PickOverlay` already exists.

**Alternatives considered:**

- Keep CPU for piercing only — rejected; user asked to delete Scheme A.

### 2. Multi-hit via GPU depth peeling

**Decision:** Add `PickOverlay::pickAllAtWindowPosition(window_x, window_y) -> eastl::vector<EntityId>`:

```
loop up to k_max_peel_count (default 16):
  run pick prepass with depth test GREATER against stored peel depth
    OR: render all, read (entity_id, depth), then CPU-sort unique entities by depth
```

Preferred implementation: **iterative peel** reusing existing prepass:

1. Clear ID + depth RTs.
2. Pass 0: normal LESS depth test → read front hit at pixel.
3. Pass N: set depth compare to GREATER with `peel_depth + epsilon`, clear only depth (keep ID clear), draw all meshes, read next hit.
4. Stop when read returns 0 or duplicate entity.
5. Deduplicate consecutive same IDs; order front-to-back.

**Rationale:** Reuses `pick_prepass.slang` with uniform `peel_depth` and compare mode flag. No full-buffer readback.

**Alternatives considered:**

- Full depth buffer CPU readback — rejected; bandwidth heavy.
- CPU raycast for piercing only — rejected per user.

### 3. Piercing menu UI (Slint)

**Decision:** New `piercing_menu.slint` — lightweight vertical list popup positioned at window coordinates. `SlintSystem` exposes `showPiercingMenu(items, x, y, add_mode)` / `hidePiercingMenu()`. Callback `on_piercing_entity_selected(EntityId)`.

**Rationale:** Matches existing editor Slint patterns (Hierarchy, Content Browser).

**Alternatives considered:**

- ImGui overlay — rejected; editor is Slint-native.

### 4. Input routing

**Decision:** Extend `RenderSystem::onEvent`:

```
Left-click (existing chain):
  gizmo → navigate gizmo → ViewportPickSystem::onViewportClick (GPU only)

Right-click:
  if Ctrl held && in viewport:
    ViewportPickSystem::onPiercingMenuClick(x, y, shift_held)
    handled = true
  else:
    EditorCamera free-look (existing)
```

Piercing menu does not run during gizmo drag.

**Rationale:** Aligns with Unity (Ctrl+right-click). Preserves Ctrl+left-click toggle.

### 5. Same-pixel click cycling

**Decision:** `ViewportPickSystem` stores `last_click_pixel` + `cycle_index`. On left-click within 3 px of last click, if peel list size > 1, select `hits[(indexOf(current) + 1) % n]` instead of front-most only.

**Rationale:** Unity parity for overlapping stacks without opening menu.

### 6. Modifier split (Ctrl left vs Ctrl right)

**Decision:** Ctrl+**left** = toggle (unchanged). Ctrl+**right** = piercing menu. No modifier conflict.

## Risks / Trade-offs

- **[Risk] Depth peel cost on Ctrl+right-click** → Cap `k_max_peel_count` (16); menu opens async after peel completes (sync fence acceptable for menu open).
- **[Risk] Peel epsilon causes missed thin geometry** → Tune `k_peel_depth_epsilon` (start `1e-4` NDC depth); test on Sponza.
- **[Risk] Slint popup focus steals input** → Dismiss on click outside or Escape; do not block camera when menu closed.
- **[Risk] Right-click + Ctrl conflicts with future context menu** → Piercing menu is the v1 Ctrl+right-click behavior; document extension point.

## Migration Plan

- Delete CPU pick code paths in same PR as peel + menu.
- Remove `BLUNDER_VIEWPORT_GPU_PICK` from docs.
- `geometry.h` ray-triangle helpers: remove if no other callers (audit shadow/nav — keep if used elsewhere).
- `MeshAsset::getLocalBounds()`: keep data, drop pick dependency.

## Open Questions

- Max peel count for dense piles (16 vs 32) — tune during apply on Sponza.
- Show entity icon/thumbnail in menu row — defer; name-only v1.
- Whether plain right-click should also offer piercing without Ctrl — Unity requires Ctrl; match Unity for v1.
