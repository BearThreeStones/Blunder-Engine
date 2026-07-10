## 1. P0 — Async pick + interaction fixes

- [x] 1.1 Add `HybridGpuPickSystem` skeleton with `requestPick` / `tryFetch` and `PickResult` (entity id, pending state)
- [x] 1.2 Refactor `PickOverlay` narrow pass to submit on pick queue + `VkFence` (no blocking readback in `pickAtWindowPosition`)
- [x] 1.3 Wire `RenderSystem` frame tick to poll pick fence and deliver results to `ViewportPickSystem`
- [x] 1.4 Implement `promotePickEntity(leaf)` using `Entity::getParentId()`; apply on all pick outcomes
- [x] 1.5 Add click-vs-drag guard (5 px threshold); skip pick after viewport camera drag
- [ ] 1.6 Manual QA: pick no longer stalls main thread; parent selected when clicking child mesh — **deferred to P2 pick_test acceptance**

> **Apply entry point:** Start implementation at **§2 P1** (skip further `fix-viewport-left-click-async-pick` work).

## 2. P1 — GPU instance buffer

- [x] 2.1 Define `PickInstanceGpu` struct + `pick_instance.h` (leaf `entity_id`, `parent_id`, world AABB min/max, flags, `gpu_mesh` key / index)
- [x] 2.2 `PickInstanceBuffer` builder: scan `SceneInstance::forEachMeshRenderer` on `m_world_matrices_dirty` or explicit pick dirty flag (NOT on click)
- [x] 2.3 Vulkan SSBO + staging upload; rebuild in `RenderSystem::tickVulkan` before pick or on `markViewportRenderDirty`
- [x] 2.4 `HybridGpuPickSystem::beginGpuPass` / `collectPickableDraws` reads cached CPU-side `PickDraw[]` from buffer rebuild (no `forEachMeshRenderer` in `requestPick` path)
- [x] 2.5 Remove or gate sync `pickAtWindowPosition` / `pickAllAtWindowPosition` from `onViewportLeftReleased` (async-only until P2 broad list)
- [ ] 2.6 Manual QA: profiler — click path no longer calls `collectPickableDraws` → `forEachMeshRenderer`

## 3. P2 — Compute broad + candidate narrow

- [x] 3.1 Add `engine/shaders/pick_broad_phase.slang` (ray vs AABB, atomic hit list cap 1024)
- [x] 3.2 Implement broad phase dispatch + hit list readback in `HybridGpuPickSystem`
- [x] 3.3 Sort/dedupe broad hits by distance; `dedupePromotedPickHits` on broad list
- [x] 3.4 Narrow raster only broad candidates; single 1×1 readback for front-most leaf → promote
- [x] 3.5 Wire piercing menu + click cycling to **broad hit list**; remove iterative depth peel hot path
- [x] 3.6 Left-click: `applySelection` on input phase when broad+narrow result ready (or sync narrow on top candidate); static-camera outline/gizmo on `pick_test`
- [x] 3.7 Update `docs/agents/render-pipeline.md` (hybrid architecture, latency table, promotion rule)

## 4. Validation

- [x] 4.1 Build `engine_runtime` / `engine_editor`
- [ ] 4.2 Manual: `pick_test` left-click `BoxFront` — Hierarchy + outline + gizmo ≤2 frames, static camera
- [ ] 4.3 Manual: same-pixel cycle `BoxMid` → `BoxBack`
- [ ] 4.4 Manual: Ctrl+right piercing menu ≥3 promoted rows
- [ ] 4.5 Manual: orbit drag release does not mis-pick
