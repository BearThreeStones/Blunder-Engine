## Context

The editor routes selection through `EditorSelectionSystem` from the Hierarchy panel and Slint `UiEvent::selectEntity`, but **viewport left-click does not pick meshes**. `EditorCamera::makeRayFromWindowPosition` and analytic gizmo picking (`transform_gizmo_pick.cpp`) already exist; `geometry.h` provides Ray-Plane, Ray-AABB, and Ray-OBB but not ray-triangle. `syncSceneToRender` iterates all `MeshRendererComponent` entries with world matrices—the same data pick needs.

`OutlineOverlay` demonstrates a GPU ID prepass pattern (mesh surface → ID buffer + depth) that Scheme B can mirror for all pickable meshes. The outline change explicitly deferred GPU picking; this change adds it as an optional scalable path alongside CPU raycast.

**Locked for v1:**

- Pick returns the **leaf entity** owning the hit `MeshRendererComponent`.
- Modifier keys match Hierarchy: click = replace, Shift = add, Ctrl = toggle.
- Blend-transparent meshes are not pickable; alpha-cutout uses forward-parity discard.
- Gizmo and navigate gizmo consume left-click before pick runs.

## Goals / Non-Goals

**Goals:**

- Left-click viewport mesh selection wired into `RenderSystem::onEvent` with correct input priority.
- **Scheme A (CPU raycast):** default path—AABB broad phase, ray-triangle narrow phase, closest hit.
- **Scheme B (GPU ID pick):** render all pickable meshes to entity-ID + depth RT; sample on click.
- Dual-path strategy: CPU default; GPU via env override or triangle-count fallback.
- Per-mesh local AABB cached on `MeshAsset` for broad phase.
- Pick miss clears selection; pick hit triggers same redraw/inspector sync as Hierarchy selection.

**Non-Goals:**

- Box / lasso / drag multi-select in the viewport (Hierarchy modifiers only for v1).
- Picking blend-transparent meshes.
- Auto-promoting leaf picks to parent entities (user selects the hit leaf).
- Physics engine integration or runtime (non-editor) picking.
- BVH acceleration structure (follow-up if CPU path is too slow before GPU fallback kicks in).
- Sharing the outline prepass RT for picks (different draw sets and lifetime).

## Decisions

### 1. New `ViewportPickSystem` in `engine/src/runtime/function/editor/`

**Decision:** Add `ViewportPickSystem` owning CPU and GPU pick orchestration. It reads `SceneInstance`, `EditorCamera`, and `EditorSelectionSystem`; it does not own rendering resources directly—GPU resources live under `PickOverlay` / `PickTargets` in the overlay module.

**Rationale:** Pick is editor input + scene query, not a render pass. Keeps `RenderSystem::onEvent` thin: delegate after gizmo checks.

**Alternatives considered:**

- Inline pick logic in `RenderSystem` — rejected; grows an already large file.
- Pick inside `EditorSelectionSystem` — rejected; selection state should not own raycast/GPU readback.

### 2. Input wiring in `RenderSystem::onEvent`

**Decision:** After transform gizmo and navigate gizmo handle the event (and only if `!event.handled`), on `MouseButtonPressed` + `SDL_BUTTON_LEFT` + position in viewport:

```
ViewportPickSystem::onViewportClick(window_x, window_y, modifier_keys)
```

**Rationale:** Matches existing gizmo priority documented in exploration and `render-pipeline.md` overlay ordering philosophy (higher-priority tools first).

### 3. Scheme A — CPU raycast algorithm

**Decision:**

1. Build `Ray` via `EditorCamera::makeRayFromWindowPosition`.
2. Iterate `SceneInstance::forEachMeshRenderer` (same set as `syncSceneToRender`).
3. Skip `cgltf_alpha_mode_blend`.
4. Broad phase: transform `MeshAsset` local AABB by `getWorldMatrix(entity_id)` → world AABB; test `intersect(ray, world_aabb)`.
5. Narrow phase: transform ray to mesh local space (`inverse(world_matrix)`); Möller–Trumbore per triangle from `MeshAsset` indices.
6. For `cgltf_alpha_mode_mask`: barycentric-interpolate UV, sample base-color alpha (or vertex alpha if no texture), discard below `alpha_cutoff`.
7. Track minimum `t > 0`; return owning `EntityId`.

Add `intersect(const Ray&, const Vec3& v0, const Vec3& v1, const Vec3& v2)` (or equivalent) to `geometry.h`.

**Rationale:** Reuses CPU mesh data already in memory; no GPU sync on click for typical scenes. Broad phase avoids O(triangles) on obvious misses.

**Alternatives considered:**

- glm `intersectRayTriangle` only — acceptable wrapper, but project prefers `geometry.h` as the math hub.
- Pick only root entities with scene-level bounds — rejected; no per-entity bounds today.

### 4. `MeshAsset` local AABB cache

**Decision:** Add `AABB m_local_bounds` (or min/max `Vec3`) computed once in the `MeshAsset` constructor from vertex positions. Expose `getLocalBounds()`.

**Rationale:** Avoid recomputing bounds every pick. Import path already has all vertices.

### 5. Scheme B — `PickOverlay` + `PickTargets`

**Decision:** Mirror `OutlineOverlay` / `OutlineTargets` with a separate pick prepass:

| Attachment | Format | Purpose |
|------------|--------|---------|
| `entity_id` | `VK_FORMAT_R32_UINT` | `EntityId` as uint (background = 0) |
| `pick_depth` | `VK_FORMAT_D32_SFLOAT` | Depth at picked surface |

- Prepass draws **all pickable** mesh renderers (opaque + mask, not blend).
- Fragment shader: write `entity_id`; alpha-cutout uses same discard as forward (`basic.slang` cutoff logic ported or shared).
- **On-demand render:** GPU pick pass runs only when a click requests GPU path, not every frame (unlike outline).

**Rationale:** `R32_UINT` fits full `EntityId` range; outline's `R16_UINT` packed IDs are for color/ob encoding, not 1:1 entity mapping. On-demand avoids per-frame cost when CPU path suffices.

**Alternatives considered:**

- Reuse `OutlineTargets` — rejected; outline draws selected subset only; pick needs all meshes; lifetimes differ.
- Every-frame GPU pick buffer — rejected for editor v1 cost.

### 6. GPU pick readback

**Decision:** After on-demand prepass, copy 1×1 region (click pixel) from `entity_id` image to a host-visible buffer via `vkCmdCopyImageToBuffer`; fence-wait (pick is synchronous from user POV, acceptable for click). Map buffer → `EntityId`.

**Rationale:** Single-pixel readback is cheap. Full-buffer readback unnecessary.

**Alternatives considered:**

- Async pick next frame — rejected; selection would feel laggy.

### 7. Dual-path strategy

**Decision:**

| Condition | Path |
|-----------|------|
| Default | CPU (Scheme A) |
| `BLUNDER_VIEWPORT_GPU_PICK=1` | GPU (Scheme B) |
| Pickable triangle count > `k_cpu_pick_triangle_budget` (default 500k) | GPU (Scheme B) |

`ViewportPickSystem` counts pickable triangles once per scene sync (or cached on `SceneInstance` dirty flag).

**Rationale:** CPU is simpler and has zero GPU overhead for small scenes; GPU scales for Sponza-scale imports.

### 8. Modifier key source

**Decision:** Read SDL keyboard modifier state at click time (`SDL_GetModState` or existing `WindowSystem` wrapper if present)—same approach planned for Hierarchy in `outline-drag-multiselect-smooth`.

**Rationale:** Consistent Shift/Ctrl behavior across panels.

### 9. Alpha-cutout on GPU path

**Decision:** `pick_prepass.slang` discards fragments below `alpha_cutoff` when `alpha_mode == mask`, sampling base-color texture when bound (fallback opaque if no texture).

**Rationale:** Parity with CPU path and forward pass.

## Risks / Trade-offs

- **[Risk] CPU pick stalls on huge scenes** → Triangle budget auto-fallback to GPU; env override for forced GPU.
- **[Risk] GPU pick 1-frame latency with fence wait** → Acceptable for click; document in render-pipeline.md.
- **[Risk] Leaf pick vs user expecting parent select** → Document; future "select parent" shortcut can be added without changing pick core.
- **[Risk] Duplicate mesh iteration (render sync + pick)** → Share pickable draw list cache keyed on scene generation if profiling shows hot path.
- **[Risk] EntityId 0 invalid vs background 0** → GPU prepass clears to 0; `k_invalid_entity_id` must not be 0 (already true).

## Migration Plan

- Additive editor feature; no asset or scene file migration.
- Default behavior: viewport left-click starts working (CPU path).
- Rollback: remove `ViewportPickSystem` wiring from `onEvent`; Hierarchy selection unchanged.

## Open Questions

- Exact `k_cpu_pick_triangle_budget` — tune with Sponza during `/opsx:apply` (start 500k).
- Whether to sample alpha from material without base-color texture (vertex color only) — match forward shader capability audit during implementation.
- Inspector focus on multi-select pick: use `getPrimarySelection()` / last picked entity (align with `outline-drag-multiselect-smooth` primary rules).
