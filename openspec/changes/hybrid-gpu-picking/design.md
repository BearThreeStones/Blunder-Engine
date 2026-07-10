## Context

Blunder's viewport pick (`PickOverlay` + `ViewportPickSystem`) rasterizes **every** pickable mesh on each click via synchronous `beginImmediateCommands`, blocking until 1×1 readback. Multi-hit uses up to 16 full-screen depth-peel passes. Cost grows linearly with instance count and stalls the main thread.

Recent fixes (`fix-gpu-pick-coords-piercing-menu`) corrected HiDPI pixel mapping and piercing-menu UI, but the fundamental scalability model is unchanged.

**Blocked band-aids (2026-07-01):** `fix-viewport-left-click-async-pick` tried async-first and restored sync-then-async; both failed manual QA on `pick_test.scene.asset` (outline/gizmo lag, cycle broken). **Decision:** stop peel/sync path patches; proceed with **P1 → P2** under this change. Piercing menu async peel works at the overlap pixel — reuse `HybridGpuPickSystem`, replace click-time `collectPickableDraws` / sync peel.

**P2 UX constraint (from archived 10.4):** Final selection for left-click MUST run in the **input phase** (or pre-`tickVulkan` flush), not only inside `pollHybridPick`, so Slint marks viewport dirty before composite. P2 broad list can fill `m_last_peel_hits` synchronously after broad readback on release, or cycle uses broad list from prior request — document in P2 tasks.

**User-locked semantics:** Scene-root-child promotion (`promotePickEntity` walk-up) is implemented in `fix-pick-promote-scene-root`; P1/P2 instance rows still store **leaf** `entity_id` for narrow raster; promotion stays on CPU after hit.

**Reference:** ASTRYN Hybrid GPU Picking — Compute broad phase (ray vs AABB) + raster narrow phase (candidates only) + async fence (~1 frame mesh latency, zero-frame gizmo).

## Goals / Non-Goals

**Goals:**

- Deliver **P0 → P1 → P2** as independently shippable milestones.
- **P0:** Async fence pick; click-vs-drag guard; parent promotion on results.
- **P1:** GPU-resident pick instance buffer; no per-click `forEachMeshRenderer` scan.
- **P2:** Slang compute broad phase; narrow raster on ≤1024 candidates; broad hit list replaces depth peel for multi-hit.
- Keep gizmo / navigate gizmo **zero-frame** CPU priority ahead of GPU pick.
- Preserve modifier semantics (replace / Shift add / Ctrl toggle) and piercing menu (Ctrl+right-click).

**Non-Goals:**

- Spatial BVH or two-level broad phase beyond 1024 cap (follow-up if needed).
- Picking through disabled/hidden layers (no layer system).
- Promoting to root ancestor or "logical group" beyond **immediate parent**.
- CPU ray-triangle fallback.
- Multi-viewport or editor play-mode pick differences.

## Decisions

### 1. Phased delivery (P0 → P1 → P2)

**Decision:** Ship three milestones; each leaves the editor usable.

| Phase | Scope | Pick behavior |
|-------|--------|----------------|
| **P0** | Async submit + fence fetch; drag threshold; parent promotion | Same full-scene narrow raster as today, **non-blocking** (~1 frame latency) |
| **P1** | GPU instance SSBO/table rebuilt on scene dirty | Narrow still full-scene initially OR filtered from buffer flags; **no click-time scene scan** |
| **P2** | Compute broad + candidate narrow | Broad hit list + single narrow 1×1; peel removed from hot path |

**Rationale:** P0 fixes stalls and parent semantics without new shaders. P1 removes CPU O(N) on click. P2 achieves million-instance broad-phase scalability.

### 2. Parent entity promotion (immediate hierarchy parent)

**Decision:** Introduce `promotePickEntity(EntityId leaf) -> EntityId`:

```
if leaf valid and scene.getEntity(leaf).getParentId() valid:
  return parent_id
return leaf
```

Apply promotion to: left-click selection, piercing menu item ids/names, click-cycle list (dedupe after promotion).

**Rationale:** User confirmed 父 mesh = hierarchy parent node. Matches Unity-style "select parent" expectations when clicking child geometry.

**Alternatives considered:**

- Leaf-only (current) — rejected by user.
- Walk to root / topmost ancestor — rejected; immediate parent only.

### 3. P0 async pick API (Vulkan)

**Decision:** Add `HybridGpuPickSystem` (or refactor `PickOverlay`) with:

```
void requestPick(float window_x, float window_y, PickRequestKind kind);
PickFetchStatus tryFetch(PickResult& out);  // idle | pending | ready
```

- `requestPick` records ray/pixel, submits narrow pass + copy to readback on **pick command queue**, signals `VkFence`.
- `tryFetch` called from `ViewportPickSystem` or frame tick: `vkWaitForFences` with timeout 0; map readback when ready.
- Gizmo hits **never** call `requestPick`; return synchronously from existing controllers.

**Rationale:** Matches ASTRYN one-frame latency without blocking SDL event thread.

**Alternatives considered:**

- Keep synchronous immediate commands — rejected (main-thread stall).

### 4. Click vs drag guard

**Decision:** Track left-press origin in viewport; if pointer moves > **5 px** before release, mark drag. On release after viewport orbit/pan (middle/right) or left-drag camera interaction, **do not** fire pick. Left-click pick only when movement ≤ threshold.

**Rationale:** ASTRYN §07; prevents rotate-then-release mis-pick.

### 5. P1 GPU instance buffer

**Decision:** Structure per pickable instance (one row per `MeshRendererComponent`):

| Field | Purpose |
|-------|---------|
| `entity_id` | Leaf entity owning renderer |
| `parent_id` | `Entity::getParentId()` for promotion |
| `world_aabb_min/max` | Broad phase + debug |
| `flags` | pickable, alpha_mode bits |
| `mesh_handle` / `gpu_mesh_id` | Narrow phase draw lookup |

Rebuild buffer when `SceneInstance` marks transforms/mesh dirty or generation changes. Upload via staging buffer once per dirty frame, not per click.

**Rationale:** Decouples pick from scene graph traversal on input.

### 6. P2 Compute broad phase (Slang + Vulkan)

**Decision:** `pick_broad_phase.slang` compute shader:

- **Input:** ray origin/dir (world), instance SSBO, instance count.
- **Per thread:** ray-AABB test (reuse `geometry.h` math on CPU for validation tests only).
- **Output:** `HitListHeader { uint hit_count; }` + `HitRecord { uint entity_id; float t; }` array cap **1024**.
- **Atomics:** `atomicAdd` on count; append record with AABB entry distance `t`. Optional `atomicMin` for nearest (debug).

Dispatch: `(instance_count + 63) / 64` workgroups.

**Rationale:** O(N) parallel on GPU; N=1M feasible at sub-ms on discrete GPU (ASTRYN ~0.3ms).

### 7. P2 Narrow phase on candidates only

**Decision:** After CPU read of hit list (sorted by `t` ascending, dedupe by **promoted** entity id):

1. Take up to `k_max_broad_hits` (1024) records.
2. Raster `pick_prepass` **only** meshes for those leaf entities (alpha-cutout unchanged).
3. Single 1×1 readback → leaf entity → **promote** → final selection.

**Multi-hit (piercing menu / cycling):** Use sorted broad list (promoted, deduped) as hit stack. **Remove** iterative depth peel from hot path in P2.

**Rationale:** Peel cost is O(layers × full scene); broad list is O(candidates) with one narrow pass.

### 8. Gizmo picking unchanged

**Decision:** `TransformGizmoController` and `NavigateGizmoOverlay` remain first in `RenderSystem::onEvent`; no GPU pick when gizmo consumes event.

**Rationale:** Zero-frame gizmo per ASTRYN; already implemented.

### 9. Module placement

**Decision:** New `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.{h,cpp}` owning broad+narrow+fence state. `PickOverlay` narrow raster becomes a dependency or merges into this type. `OverlaySystem` exposes `requestPick` / `tryFetch`.

**Rationale:** Clear boundary; `PickTargets` FBO stays in overlay.

## Risks / Trade-offs

- **[Risk] 1-frame pick latency after P0** → Acceptable per ASTRYN; gizmo stays instant; document in `render-pipeline.md`.
- **[Risk] Parent promotion hides leaf-specific inspector** → Expected; user selects parent by design. Inspector shows parent transform.
- **[Risk] Broad phase 1024 cap truncates dense piles** → Log/debug counter; future spatial partition. Piercing menu shows up to 1024 entries.
- **[Risk] AABB false positives in menu list** → Narrow phase confirms front-most; menu may list broad candidates slightly behind pixel—optional P2.1 narrow confirm per row deferred.
- **[Risk] First compute shader in pick path** → Add validation mode comparing broad candidates vs full narrow in dev builds.
- **[Risk] Instance buffer stale one frame** → Tie rebuild to `viewport_render_generation` / scene dirty generation.

## Migration Plan

1. **P0** lands without removing peel; async + parent promotion + drag guard.
2. **P1** switches draw collection to GPU buffer; peel/async narrow unchanged.
3. **P2** enables compute broad; delete peel loops from `PickOverlay::pickAllAtWindowPosition`; update `gpu-pick-piercing-menu` docs cross-reference.
4. Archive `hybrid-gpu-picking` after manual QA on overlapping meshes + deep hierarchy.
5. Rollback: feature flag `BLUNDER_HYBRID_PICK=0` to sync narrow (dev only) if needed during P0 bake-in.

## Open Questions

- Should broad-phase hit list entries show **parent name** only, or `ChildName (ParentName)` for clarity?
- Instance buffer: pack all scenes or active `SceneInstance` only? (Default: active instance.)
- P2 narrow: one full-size pass with all candidates vs instanced single draw? (Default: iterate candidates like today, count ≤1024.)
