## Why

Play Mode’s Forward Player has no participating media: distant meshes and empty sky stay the same brightness as near surfaces. Authors need camera-aligned volumetric fog on the existing Viewport present path (editor Deferred lighting or Player Forward) — not a new GBuffer, and not a second 2D height-fog pass.

## What Changes

- Add a Unique **Fog Component** (like Camera / Light): enable volumetric fog, height density, Z-up falloff, view distance, albedo, Henyey–Greenstein `g`. Add… Fog does not create an Object. Missing Fog means no volumetric fog (existing scenes stay clear).
- Build a **camera-aligned 3D froxel volume** for the presented view: ~**16 px** screen tiles × **64** exponential-Z slices (Unreal-style `S = 32`), density from the Fog Component at each cell’s world **Z**.
- Inject the scene’s **unshadowed Directional** into that volume; Z-integrate in-scatter + transmittance; **thin temporal** (~20% current / 80% history). Composite on the presented color target: `color * T + inscatter`.
- **Editor Viewport uses the same composite as Player** (Grill amendment). If Viewport is Deferred, still composite that volume on the presented color — do not invent a second GBuffer. Camera Preview, Placement Preview, Mesh Preview, and Scene Thumbnail stay unfogged.
- **No volumetric shadows. No point/spot inject** (Player has no clustered light list). No analytical 2D height-fog pass. No new GBuffer.
- **Do not amend** in-flight `mesh-shader-shadows` or archived clustered-deferred / gpu-driven changes.

## User stories

1. **Viewport and Player show volumetric fog** — I open a scene that has Fog enabled and see distance fog on lit meshes and empty sky in the editor Viewport without Play, and in the Player game view (including Headless Play frames). The look is the same composite `color * T + inscatter`.
2. **Camera 3D volume, Z-up height** — Fog lives in a camera-aligned 3D volume of about 16-pixel screen tiles by 64 exponential Z slices. Density falls off with world Z (up). There is no separate 2D height-fog fullscreen pass.
3. **Unshadowed directional, thin temporal** — The volume is lit by the scene Directional with no volumetric shadows. A thin temporal blend (~20% current) calms slice banding. Point and spot lights do not inject (Player has no clustered list).
4. **Stack on existing present path** — Fog stacks on the existing Viewport present path (Deferred lighting in editor, Forward in Player). No new GBuffer. GPU apply waits until the Windows dirty tree is free.

## Capabilities

### New Capabilities

- `scene-fog-component`: Fog Component document model, Unique Add…, Inspector section, density / falloff / view distance / albedo / `g`, enable gate, first-active Fog in EntityId order
- `player-volumetric-fog`: Viewport + Player froxel volume (16 px × 64 Z, exponential `S = 32`), Z-up height density, unshadowed directional inject, thin temporal, composite `color * T + inscatter` on the presented color

### Modified Capabilities

- `play-player`: Player color target (windowed and Headless Play frame) composites volumetric fog when Fog is enabled
- `inspector-add-menu`: Fog is a Unique attachment; Add… Fog does not create an Object
- `inspector-present-only-sections`: Fog Inspector section is present-only
- `inspector-add-kind-icons`: Fog has one Add… kind icon
- `hierarchy-row-icons`: present Fog Unique shows a Hierarchy row icon
- `scene-edit-commands`: Add… / Remove Fog and Fog property edits are Document History Commands
- `shader-resource-layout`: volumetric-fog compute and composite keep hand-written layouts (same class as SSAO / pick compute)
- `bindless-texture-table`: volumetric-fog 3D textures stay off the Bindless color table

## Impact

- `SceneInstance` / scene serializer (`"fog"` object); Inspector Add… Unique Fog; Hierarchy row icon
- `RenderSystem` `recordViewportGraph`: volume compute + composite after Viewport lighting / Forward, gated on an active Fog (Editor and Player hosts)
- New pass under `engine/src/runtime/function/render/` (SSAO-like) + Slang compute/composite shaders
- Scene Light gather: one illuminating Directional for inject (existing Light Component; no clustered list)
- Tests: serializer, Z/height CPU mirrors, host gate, Fog pick order
- Glossary: Fog Component / volumetric fog in `CONTEXT.md`
