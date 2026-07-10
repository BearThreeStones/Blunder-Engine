## 1. Math and mesh data (Scheme A foundation)

- [x] 1.1 Add `intersect(ray, triangle)` (Möller–Trumbore) to `geometry.h` with `RayHit` output
- [x] 1.2 Add local AABB cache to `MeshAsset` (compute from vertices in constructor); expose `getLocalBounds()`
- [x] 1.3 Audit mesh import paths to ensure all `MeshAsset` construction goes through the constructor that computes bounds

## 2. ViewportPickSystem — CPU path (Scheme A)

- [x] 2.1 Add `viewport_pick_system.h/.cpp` under `engine/src/runtime/function/editor/`
- [x] 2.2 Implement `pickCpu(Ray, SceneInstance*)` — iterate `forEachMeshRenderer`, skip blend, AABB broad phase, ray-triangle narrow phase, closest `t`
- [x] 2.3 Implement alpha-cutout discard in CPU narrow phase (UV barycentric + base-color alpha vs `alpha_cutoff`)
- [x] 2.4 Implement `onViewportClick(window_x, window_y, modifiers)` — build ray from `EditorCamera`, run CPU pick, apply `setSelection` / `addToSelection` / `toggleSelection` / `clearSelection`
- [x] 2.5 Register `ViewportPickSystem` in global context or inject via existing editor service handles
- [x] 2.6 Cache pickable triangle count per scene generation for dual-path strategy

## 3. Input integration

- [x] 3.1 Wire `RenderSystem::onEvent`: after gizmo + navigate gizmo, on left-click in viewport call `ViewportPickSystem::onViewportClick` when `!event.handled`
- [x] 3.2 Read Shift/Ctrl modifier state at click time (SDL mod state or `WindowSystem` wrapper)
- [x] 3.3 Ensure pick hit/miss triggers `requestViewportRedraw` and inspector/hierarchy dirty sync (match Hierarchy selection side effects)

## 4. GPU pick resources (Scheme B)

- [x] 4.1 Add `PickTargets` (`pick_targets.h/.cpp`) — `R32_UINT` entity-ID image, `D32` depth, render pass, framebuffer (mirror `OutlineTargets` patterns)
- [x] 4.2 Add `pick_prepass.slang` — VS (model/view/proj) + FS writing `EntityId` with depth test/write and alpha-cutout discard
- [x] 4.3 Add `PickOverlay` (`pick_overlay.h/.cpp`) — prepass draw all pickable meshes, on-demand only
- [x] 4.4 Implement 1×1 pixel GPU readback (copy image to host buffer, fence-wait, map → `EntityId`)
- [x] 4.5 Extend `OverlaySystem` to own `PickTargets` + `PickOverlay` (init/resize/shutdown)

## 5. ViewportPickSystem — GPU path (Scheme B)

- [x] 5.1 Implement `pickGpu(pixel_x, pixel_y)` — trigger on-demand prepass, read back entity ID at viewport-local pixel
- [x] 5.2 Implement dual-path routing: default CPU; `BLUNDER_VIEWPORT_GPU_PICK=1` forces GPU; triangle budget exceeded auto-fallback
- [x] 5.3 Verify CPU and GPU paths return the same entity for opaque and alpha-cutout meshes on a test scene

## 6. Validation and docs

- [x] 6.1 Build `engine_runtime` (or project validate script)
- [ ] 6.2 Manual: left-click mesh in viewport → entity selected, outline + gizmo update
- [ ] 6.3 Manual: left-click empty viewport → selection cleared
- [ ] 6.4 Manual: gizmo handle click → selection unchanged, gizmo drag works
- [ ] 6.5 Manual: Shift+click / Ctrl+click → add / toggle selection (multi-select outline if enabled)
- [ ] 6.6 Manual: blend-transparent mesh → click passes through to mesh behind
- [ ] 6.7 Manual: `BLUNDER_VIEWPORT_GPU_PICK=1` → GPU path selects correctly on dense scene (e.g. Sponza)
- [x] 6.8 Update `docs/agents/render-pipeline.md` — viewport pick input priority, CPU vs GPU paths, env vars
