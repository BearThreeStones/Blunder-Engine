## Why

Manual QA on `pick_test.scene.asset` confirms piercing menu and same-pixel click cycling are broken: Ctrl+right-click lists only one row (`node`), repeat left-click does not cycle, and selection stays on a deep glTF child. Root cause: **GPU depth peel** (`pick_prepass.slang` + `HybridGpuPickSystem` peel chain) uses a **standard near=0 depth peel comparison**, while the editor camera uses **`glm::perspectiveZO`** (Vulkan zero-to-one, near→1, far→0). The second peel pass discards all behind-surface hits, so `peel_hits` never grows beyond one entry—blocking piercing menu and click cycling until P2 broad-phase lands.

## What Changes

- Fix pick prepass **depth peel discard** and align **depth buffer clear / compare op** with the engine's ZO projection so iterative peel returns front-to-back leaf hits at a pixel.
- Keep existing async peel orchestration in `HybridGpuPickSystem` (P0 path); no broad-phase compute in this change.
- Add a **regression check** using `PickOverlay::pickAllAtWindowPosition` (sync peel loop) and manual QA on `pick_test` after geometry overlap is corrected.
- Adjust **`pick_test.scene.asset`** box placement so three cubes truly overlap along the view ray at the QA camera pose (current 1.2 m spacing leaves ~0.2 m gaps between 1 m cubes).
- Document depth convention in `docs/agents/render-pipeline.md` (ZO + peel).

**Out of scope (follow-up change):** parent promotion to scene root entity (`BoxBack` vs inner `node`); P2 broad-phase replacement of peel (`hybrid-gpu-picking`).

## Capabilities

### New Capabilities

- _(none)_

### Modified Capabilities

- `viewport-piercing-menu`: Multi-hit GPU depth peel SHALL return all distinct promoted hits along the ray when geometry overlaps at a pixel, using depth semantics consistent with `perspectiveZO`.
- `viewport-mesh-pick`: Same-pixel click cycling SHALL advance through the peeled hit list when multiple pickable surfaces exist at the pixel.

## Impact

- `engine/shaders/pick_prepass.slang` — peel discard comparison
- `engine/src/runtime/function/render/overlay/pick_overlay.cpp` — depth clear / compare op (if required for consistency)
- `engine/src/runtime/function/render/pick/hybrid_gpu_pick_system.cpp` — peel termination / epsilon (verify only)
- `Assets/Scenes/pick_test.scene.asset` — overlapping QA layout
- `docs/agents/render-pipeline.md` — peel depth convention note
- Related active changes: unblocks manual tasks in `hybrid-gpu-picking`, `pick-test-scene` (task 5.2), `gpu-pick-piercing-menu`
