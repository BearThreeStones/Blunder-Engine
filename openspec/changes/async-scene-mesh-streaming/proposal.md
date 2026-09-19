# Proposal

## Why

Opening a Scene Asset still stalls `SceneSystem::loadScene` / `instantiateScene` / `completeSceneDocumentInstantiate` → `attachEntityMeshes` until unique Mesh Assets are CPU-resident and `syncSceneToRender` has called `getOrUploadGpuMesh` (`GpuMesh::create`, mapped `CPU_TO_GPU`) for every unique mesh. DogWalk `se-world.scene.asset` is ~10828 entities / ~10794 MeshRenderers / **252 unique Mesh Asset GUIDs**; the editor shell and `engine_player` should **Iterate** before those 252 CPU reads and GPU uploads finish. Texture Loader already streams color textures ([ADR 0058](../../../docs/adr/0058-async-texture-loader.md)); this change is the matching **unique Mesh Asset** residency path. Decision: [ADR 0072](../../../docs/adr/0072-async-scene-mesh-streaming.md).

## What Changes

- **Engine-wide Mesh Loader** on the same world-build path every scene already uses: `SceneSystem::loadScene`, editor `EditorSceneEditSystem::openScene`, Play `engine_player`. Not a DogWalk-only flag. Not a second editor “fake stream.”
- **Entities stay synchronous.** Scene Asset deserialize + `SceneInstance::instantiate` build the entity table (parents, cameras, unique names) before the first Iterate. No entity paging, no second PackedScene, no nested Scene Assets.
- **Unique Mesh Assets stream.** CPU read of cooked `.meshbin` / Fast Path is Jobs over Job data. `GpuMesh` upload is RHI-tick work with a per-tick budget. Jobs do not call RHI. `MeshRenderer` binds when that GUID’s CPU Mesh is ready; draws skip until that GUID has a `GpuMesh`. Coalesce by Mesh Asset GUID. Job submit order is **document order** of first GUID appearance (frustum priority is a later knife).
- **First frame vs complete.** First frame: window / Viewport / Player can Iterate; the forest may be empty or sparse. Complete (this slice): MeshRenderers for unique meshes that loaded are bound, those `GpuMesh` objects are uploaded, deferred glTF material leftover is 0. Texture Loader copies MAY still be in flight. No new loading UI. No hard first-frame millisecond gate. Flatten’s `se_world_flatten_test` `<10s` sync gate stays flatten’s, not this acceptance.
- **Keep GUID Mesh bind.** Unique Mesh Asset GUID / `.mesh.yaml` is the streaming unit. Do **not** stream the per-instance glTF importer (`openGltfImportDocument` ~10k times). That importer is the collision product exe path and is out of this change.
- **Skip, do not empty.** A failed unique Mesh skips that GUID’s renderers; the rest of the scene stays. Metres, Z-up, negative scale unchanged. No Sponza `0.008` / `kMeterSpaceTranslationMaxAbs`.
- **Do not redo Texture Loader or material hydrate.** Sidecar materials bind with the CPU Mesh; do not use 2/frame hydrate as the open policy. Checkerboard albedo while Texture Loader flies is existing behavior, not a failure.
- **No binary Scene Asset this slice.** If JSON+instantiate still dominate open feel, that is a later knife.

**Out of scope:** entity chunk/distance streaming, world paging, Nanite-style meshlet disk stream, nested Scene Assets, runtime `instance_asset_id`, Texture Loader rewrite, thumbnail 2/tick or Hierarchy fold as “scene stream,” streaming the collision-branch ~10k glTF reimport, flatten apply tail ([#31](https://github.com/BearThreeStones/Blunder-Engine/pull/31) read-only), dog-on-ground, static trimesh, C# Find/group/ray, `cursor/scene-collision-bridge-8a89` / [#24](https://github.com/BearThreeStones/Blunder-Engine/pull/24), Test/Sponza as the forest, ikea/zoo/vertical_slice, loading-bar UI, frustum-priority queue, profiler, 138 Spot, paper look.

## User stories

1. Open the DogWalk main scene `se-world.scene.asset`; the editor window can orbit and click. The first frame does not wait until all unique Mesh Assets are CPU+GPU resident.
2. In the Viewport the forest grows as unique meshes become resident: eventually the real trees, fence, and ground GEO (including grass / bush / reed in that suite), not eight empty nodes and not placeholder boxes; units are metres, Z-up.
3. Play (`engine_player`) enters the same scene: the first frame is also not blocked on all-mesh residency; when complete it draws the same forest (existing GPU-driven).
4. If one unique Mesh fails, only that class is missing; the rest of the scene is not emptied.
5. The main scene is in DogWalk; Test and Sponza are not the forest. Collision QC stays on the existing `root.scene.asset`.
6. The dog does not need to walk on this ground. C# Find / group / ray and collision wireframe are not required. First-frame checkerboard albedo (Texture Loader still in flight) is allowed and is not a failure.

## Capabilities

### New Capabilities

- `async-scene-mesh-streaming`: Mesh Loader — unique Mesh Asset CPU Jobs, tick-budget `GpuMesh` on the RHI thread, GUID coalesce, document-order Jobs, skip-draw until resident, fail-skip one GUID, generation cancel on scene drop, Headless skips GPU without a device. Engine-wide on `loadScene` / `openScene` / `engine_player`. Entities stay sync. No new loading UI. No binary Scene Asset. No first-frame ms gate.
- `dogwalk-se-world-mesh-stream`: Human acceptance on DogWalk `assets/Scenes/se-world.scene.asset` (~10828 / ~10794 / 252 GUID) with GUID Mesh bind. Not Test/Sponza. Collision QC stays on `root.scene.asset`. Not the collision exe’s ~10k glTF reimport.

### Modified Capabilities

- `play-player`: `engine_player` SHALL enter SDL Iterate with the entry scene’s entity table instantiated; it SHALL NOT treat “all unique Mesh CPU+GPU residency finished” as the Play open gate. Play streams in its own process; it SHALL NOT receive Live editor pointers.
- `gpu-driven-rendering`: Viewport / Player SHALL skip a MeshRenderer whose unique `GpuMesh` is not yet resident. One `syncSceneToRender` SHALL NOT synchronously `GpuMesh::create` every unique mesh in the open scene. Resident static Meshlet meshes still use the existing GPU-driven path. No MultiMesh Unique.

## Impact

- **Engine:** Mesh Loader beside Texture Loader (CPU Jobs + RHI-tick GPU budget). `loadScene` / `openScene` return after entity+camera instantiate and unique-mesh enqueue. `attachEntityMeshes` GUID bind stays; `syncSceneToRender` uses resident `GpuMesh` only (no all-unique sync upload). Player boot must not wait 252 meshbins before Iterate.
- **Tests:** synthetic multi-GUID fixture in this repo (coalesce, fail-skip, document order, loadScene returns before all CPU Jobs finish). Not the 10k-entity Godot tree. Not collision QC `root.scene.asset`.
- **Content:** Human walk is DogWalk `E:\Blunder Projects\DogWalk` `Assets/Scenes/se-world.scene.asset` with GUID bind (flatten product). This planning PR does not bake, does not apply flatten, and does not commit on [#31](https://github.com/BearThreeStones/Blunder-Engine/pull/31) or `cursor/scene-collision-bridge-8a89` / [#24](https://github.com/BearThreeStones/Blunder-Engine/pull/24).
- **Docs:** CONTEXT Mesh Loader; [ADR 0072](../../../docs/adr/0072-async-scene-mesh-streaming.md).
- **This planning change** is OpenSpec artifacts + glossary/ADR only — no engine/GPU C++ until `/opsx:apply`.
