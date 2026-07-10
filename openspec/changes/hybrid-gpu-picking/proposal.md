## Why

The current GPU pick path (`PickOverlay`) performs a **synchronous full-scene raster prepass** on every click—`collectPickableDraws` iterates all mesh renderers on CPU, then draws every pickable mesh, blocking on `beginImmediateCommands` until readback completes. Multi-hit uses up to **16 full-screen depth-peel passes**. This does not scale beyond modest scene sizes and causes main-thread stalls.

An ASTRYN-style **hybrid** architecture—Compute broad phase (ray vs instance AABB) + raster narrow phase (entity-ID on candidates only) + **async fence** (one-frame latency)—keeps pick cost bounded as instance count grows toward hundreds of thousands or millions.

Additionally, picks today resolve to the **leaf** `MeshRendererComponent` entity. Users expect **hierarchy parent** selection: clicking a child mesh should select its parent entity in the scene tree when a parent exists.

## What Changes

Phased delivery (**P0 → P1 → P2**); each phase is shippable independently.

### P0 — Async pick (same accuracy, no stall)

- Replace synchronous `beginImmediateCommands` pick with **request / fetch** API backed by `VkFence` (one-frame latency for mesh pick).
- Gizmo / navigate gizmo remain **zero-frame** CPU hits (unchanged priority).
- Add **click vs drag** guard (default 5 px): do not pick on left-button release after viewport camera drag.

### P1 — GPU pick instance buffer

- Maintain a **GPU-resident instance table** per active scene: `EntityId`, parent `EntityId`, world AABB, pickable flags, generation/dirty tag.
- Rebuild or incrementally update on scene/mesh/transform dirty (not on every click).
- Narrow phase reads draw list from buffer metadata instead of scanning `SceneInstance` on click.

### P2 — Compute broad phase + candidate narrow phase

- **Broad phase:** Slang compute shader, ray vs world AABB per instance; SSBO hit list (cap **1024**), `atomicAdd` append + distance for sort.
- **Narrow phase:** Raster `pick_prepass` only for broad-phase **candidates** (not full scene); single 1×1 readback for front-most entity ID + depth.
- **Multi-hit** (piercing menu, same-pixel cycling): use **sorted broad hit list** (front-to-back, dedupe by promoted entity); remove iterative **depth peel** as the primary multi-hit path.
- **Parent promotion:** After narrow hit on leaf entity `L`, if `L` has a valid parent `P` in the active scene, selection and piercing-menu rows SHALL target **`P`** (immediate hierarchy parent). Deduplicate menu/cycle list by promoted id.

## Capabilities

### New Capabilities

- `hybrid-gpu-picking`: Async fence pick lifecycle, GPU instance buffer, compute broad phase, candidate-only narrow phase, parent-entity promotion.

### Modified Capabilities

- `viewport-mesh-pick`: Pick implementation becomes hybrid async (P0–P2); parent hierarchy selection replaces leaf-only; broad+narrow replaces full-scene synchronous raster.
- `viewport-piercing-menu`: Multi-hit source becomes broad-phase hit list (not depth peel); menu rows show promoted parent entity names.

## Impact

- `engine/src/runtime/function/render/overlay/pick_overlay.{h,cpp}` — refactor to `HybridGpuPickSystem` or extend; async submit/fetch
- **New:** `engine/shaders/pick_broad_phase.slang` (compute), instance SSBO layouts
- `engine/src/runtime/function/editor/viewport_pick_system.{h,cpp}` — async fetch, parent promotion, drag threshold
- `engine/src/runtime/function/render/render_system.{h,cpp}` — integrate pick into frame graph; fence polling
- `engine/src/runtime/function/scene/scene_instance.{h,cpp}` — parent id / bounds export for instance buffer
- `docs/agents/render-pipeline.md` — hybrid pick architecture, latency table, parent selection rule
- **Supersedes** hot-path reliance on depth-peel multi-hit from `gpu-pick-piercing-menu` (peel code may be removed in P2)
