## Why

The editor has entity selection and a transform gizmo, but no visual feedback on the selected mesh in the 3D viewport. Users cannot see which object is selected at a glance—especially on busy scenes or when the gizmo is hidden. Blender solves this with a two-pass GPU outline in the Overlay engine; Blunder already has an `OverlaySystem` skeleton and a stub `WireframeOverlay`, but no silhouette outline pass exists yet.

## What Changes

- Add **Outline Selected** rendering: a prepass that writes object IDs for the selected entity, then a fullscreen resolve pass that detects silhouette edges and composites an outline onto the viewport color buffer.
- **Default on** whenever an entity is selected (no UI toggle in v1; no env-var gate).
- **Hardcoded Blender-style orange** for the default selected outline color (transform-drag variant deferred to a follow-up).
- **Opaque and transparent** selected meshes both participate in the prepass (surface geometry for both alpha modes).
- New `OutlineOverlay` (separate from `WireframeOverlay`) integrated into the existing overlay phase ordering: after the forward scene pass, before overlay line AA / SSAO.
- P0 uses **mesh surface prepass** (silhouette edges via ID discontinuity)—not X-Ray edge-detection geometry.

## Capabilities

### New Capabilities

- `viewport-selection-outline`: Editor viewport outline for the selected scene entity—prepass ID buffer, edge resolve, depth occlusion, and pipeline integration.

### Modified Capabilities

_(none — no existing main specs)_

## Impact

- `engine/src/runtime/function/render/overlay/` — new `OutlineOverlay`, `OutlineTargets`, shader glue; `OverlaySystem` orchestration
- `engine/shaders/` — `outline_prepass.slang`, `outline_resolve.slang`
- `engine/src/runtime/function/render/render_system.cpp` — insert outline draw calls in the frame tick
- `engine/src/runtime/function/render/overlay/overlay_state.h` — selection context for outline sync
- `docs/agents/render-pipeline.md` — document new overlay phase (when implementation lands)
- No changes to PBR forward shaders, `GpuMesh` upload, or `EditorSelectionSystem` API
