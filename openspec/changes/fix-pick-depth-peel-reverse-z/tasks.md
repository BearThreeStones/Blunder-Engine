## 1. Depth convention audit

- [x] 1.1 Compare main viewport depth clear/compare with pick prepass (`pick_overlay.cpp`); document finding in code comment or `render-pipeline.md`
- [x] 1.2 Align pick prepass depth clear and `VK_COMPARE_OP_*` with `glm::perspectiveZO` front-most semantics

## 2. Peel shader fix

- [x] 2.1 Update `engine/shaders/pick_prepass.slang` peel discard for ZO (nearer = greater depth): discard `z >= peelDepth - epsilon` on peel passes
- [x] 2.2 Verify `peelDepth` / `peelEpsilon` uniform wiring in `PickOverlay::drawPickPrepass` matches shader

## 3. Peel chain verification

- [x] 3.1 Confirm sync `PickOverlay::pickAllAtWindowPosition` returns multiple hits on overlapping geometry after shader/pipeline fix
- [x] 3.2 Smoke-test async `HybridGpuPickSystem` piercing_menu peel chain (no logic change unless epsilon tuning needed)

## 4. Pick-test QA scene

- [x] 4.1 Update `Assets/Scenes/pick_test.scene.asset` so three boxes overlap along the default focus camera ray (shared center or spacing ≤ cube extent)
- [x] 4.2 Update pick-test table in `docs/agents/render-pipeline.md`

## 5. Build and manual validation

- [x] 5.1 Build `engine_editor`
- [ ] 5.2 Manual: Ctrl+right on overlapping pixel → piercing menu ≥3 rows
- [ ] 5.3 Manual: same-pixel left-click twice → selection cycles through peel list (≥3 steps when geometry overlaps)
