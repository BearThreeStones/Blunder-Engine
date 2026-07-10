## Why

The editor supports selection from the Hierarchy panel and UI events, but **left-click in the 3D viewport does not pick scene meshes**. Users must switch to the Hierarchy to select objects—a major workflow gap versus Blender and other DCC tools. The engine already has viewport ray construction (`EditorCamera::makeRayFromWindowPosition`), transform-gizmo analytic picking, and outline prepass infrastructure; viewport mesh picking is the missing bridge from click → `EditorSelectionSystem`.

## What Changes

- Add **viewport mesh picking** on left-click inside the editor viewport, integrated into the existing input priority chain (after transform gizmo and navigate gizmo, before camera orbit).
- **Scheme A (CPU raycast)** — default path: ray vs mesh triangles with local AABB broad phase; returns the closest hit entity. Ships first.
- **Scheme B (GPU ID pick pass)** — optional scalable path: render all pickable meshes to an entity-ID + depth target (reuse outline-prepass patterns); resolve pick by sampling the buffer at the click pixel. Enabled when CPU pick exceeds a triangle budget or via env toggle.
- **Pick rules**: opaque and alpha-cutout meshes are pickable; blend-transparent meshes are skipped in v1. Alpha-cutout uses the same cutoff discard semantics as the forward pass.
- **Selection semantics**: click = replace, Shift+click = add, Ctrl+click = toggle (aligned with Hierarchy / `render-pipeline.md`). Click on empty viewport clears selection.
- **Entity target**: pick returns the **hit leaf entity** (the entity owning the `MeshRendererComponent`); gizmo and outline subtree behavior remain driven by what the user selects.
- Cache **per-mesh local AABB** on `MeshAsset` at import/load for CPU broad phase.

## Capabilities

### New Capabilities

- `viewport-mesh-pick`: Editor viewport 3D mesh picking—click handling, raycast and GPU pick paths, selection modifier keys, gizmo priority, and integration with `EditorSelectionSystem`.

### Modified Capabilities

_(none — no existing main specs)_

## Impact

- `engine/src/runtime/core/math/geometry.h` — ray-triangle intersection helper
- `engine/src/runtime/resource/asset/mesh_asset.h` — cached local AABB
- `engine/src/runtime/function/editor/` — new `ViewportPickSystem` (CPU + GPU orchestration)
- `engine/src/runtime/function/render/render_system.cpp` — `onEvent` wiring; optional GPU pick prepass hook
- `engine/src/runtime/function/render/overlay/` — new `PickOverlay` / `PickTargets` (Scheme B), shader glue
- `engine/shaders/` — `pick_prepass.slang` (Scheme B)
- `docs/agents/render-pipeline.md` — document pick input priority and dual-path behavior
- No breaking changes to public game/runtime APIs; internal editor input path only
