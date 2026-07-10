## 1. Remove CPU pick path

- [x] 1.1 Remove `ViewportPickSystem::pickCpu`, `countPickableTriangles`, `shouldUseGpuPick`, triangle cache, and `k_cpu_pick_triangle_budget`
- [x] 1.2 Make `onViewportClick` always use `RenderSystem::pickEntityAtWindowPosition` (GPU)
- [x] 1.3 Remove `BLUNDER_VIEWPORT_GPU_PICK` env handling and documentation
- [x] 1.4 Audit `geometry.h` ray-triangle / pick-only helpers; remove if unused outside pick

## 2. GPU depth peel multi-hit

- [x] 2.1 Extend `pick_prepass.slang` with peel uniform (`peel_depth`, `compare_mode` or `is_peel_pass`)
- [x] 2.2 Implement `PickOverlay::pickAllAtWindowPosition` — iterative peel, return distinct `EntityId` list front-to-back
- [x] 2.3 Keep `pickEntityAtWindowPosition` as front-most hit (first peel result or single sample)
- [x] 2.4 Add `RenderSystem::pickAllEntitiesAtWindowPosition` delegating to overlay

## 3. Piercing menu (Unity-style)

- [x] 3.1 Add `piercing_menu.slint` popup (entity name list, cursor anchor)
- [x] 3.2 Wire `SlintSystem::showPiercingMenu` / hide + selection callback
- [x] 3.3 Implement `ViewportPickSystem::onPiercingMenuClick` — Ctrl+right-click replace, Ctrl+Shift+right-click add
- [x] 3.4 Wire `RenderSystem::onEvent` for Ctrl+right-click in viewport (after gizmo checks, before camera free-look)

## 4. Same-pixel click cycling

- [x] 4.1 Track last click pixel + cycle state in `ViewportPickSystem`
- [x] 4.2 On repeat left-click within tolerance, cycle peel hit list instead of always picking front-most

## 5. Validation and docs

- [x] 5.1 Build `engine_runtime` / `engine_editor`
- [ ] 5.2 Manual: left-click picks front-most mesh (GPU only)
- [ ] 5.3 Manual: Ctrl+right-click on overlapping meshes opens piercing menu; choosing entry selects
- [ ] 5.4 Manual: Ctrl+Shift+right-click add-from-menu works
- [ ] 5.5 Manual: repeat left-click cycles overlapping stack
- [x] 5.6 Update `docs/agents/render-pipeline.md` — GPU-only pick, piercing menu, remove dual-path table
