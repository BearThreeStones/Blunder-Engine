# Proposal

## Why

Godot `SE-world` is not a mesh: nodes carry `instance_asset_id` extras that `gltf_import_script.gd` expands against `asset_index.json`. Blunder’s `GltfSceneImporter` ignores those extras, so a raw import is eight empty nodes. October “forest can stand” needs the reachable SE/SL layout as **one flat Scene Asset** in DogWalk — real library glTF, metres, Z-up — before dog-on-ground and without nested scenes.

## What Changes

- **Offline bake** of Godot `SE-world.gltf` + `asset_index.json` + reachable SE/SL set and library glTFs into **one flat Scene Asset**. Runtime `instantiateScene` / `attachEntityMeshes` load that document. Runtime **does not** look up `instance_asset_id` or keep a second PackedScene / prefab graph.
- **~4795** LI/PR layout instances (196 library assets) as entities with **shared** Mesh Asset GUID refs, plus set grouping nodes and set-glTF **GEO** (ground / path / snow / pond / creek). Library-internal `instance_asset_id` (needles, knots, leaves) expand **under** that layout instance. **COL-*** get **no MeshRenderer** this slice.
- **Unique entity names** (`stem` / `stem_N`, same family as `makeUniqueEntityName`). Missing asset ids (known: `9c53197a476fa552`, eight `LI-bushlet_blue_delicate_006`) **skip** that instance; the bake does not abort into an empty scene.
- **Metres, Z-up.** Layout TRS is engine space via existing `similarityGltfToEngine` (`(x,y,z)_gltf → (x,z,−y)_engine`). Not Unreal cm, not Sponza `0.008`. Negative scale is kept. Do not classify the forest as a centimetre world with `kMeterSpaceTranslationMaxAbs`.
- **DogWalk** product scene: `Assets/Scenes/se-world.scene.asset`. Leave collision QC `Assets/Scenes/root.scene.asset` alone. Test / Sponza are not the forest.
- Play and the editor Viewport draw the forest through **existing GPU-driven** mesh submission. No MultiMesh Unique this slice.

**Out of scope:** nested Scene Assets / `childScenes`; runtime `instance_asset_id` lookup; `.tscn` import; MultiMesh Unique; dog-on-ground / static trimesh bind / TerrainIce; C# Find / group / ray; `cursor/scene-collision-bridge-8a89` and PR 24; Test / Sponza as the forest; `SE-asset_ikea` / `SE-asset_zoo` / `vertical_slice`; `world.tscn` nodes outside SE-world; 138 Spot, paper look, profiler, birds, dual character / leash / camera.

## User stories

1. Open the DogWalk main scene; the Viewport shows SE-world’s real trees, fence, and ground (including grass / bush / reed in that suite), not eight empty instance nodes and not placeholder boxes.
2. One flat `.scene.asset`: about 5000 layout instances are each an entity; meshes reference shared library glTF / Mesh Assets; set nodes may use `parent` for grouping.
3. Ground / path / snow / pond / creek enter as set-glTF GEO (and the same class of visible mesh); units are metres, Z-up.
4. Duplicate names are uniquified; instances whose asset id is missing are skipped; a failed instance does not empty the whole scene.
5. The main scene is in DogWalk; Test and Sponza are not the forest. Collision QC stays on the existing `root.scene.asset`.
6. Play and the editor both draw this forest (existing GPU-driven mesh submission). The dog does not need to walk on this ground. C# Find / group / ray are not required.

## Capabilities

### New Capabilities

- `se-world-instance-flatten`: Offline bake of Godot `instance_asset_id` (layout + nested library instances) into a flat Scene Asset; unique names; skip missing ids; COL-* have no MeshRenderer; metre Z-up; runtime does not look up `instance_asset_id`
- `dogwalk-se-world-scene`: DogWalk forest at `Assets/Scenes/se-world.scene.asset`; collision QC `root.scene.asset` untouched; not Test/Sponza; editor Viewport and Play draw it

### Modified Capabilities

- `asset-import`: glTF Import / `GltfSceneImporter` SHALL ignore `instance_asset_id` extras (no runtime `asset_index` lookup, no nested Scene Asset). Flatten skip-missing does not abort Import of other meshes.
- `asset-pull-cook`: SE/SL library and set glTFs copied into DogWalk SHALL register as Mesh Assets (GUID). The Scene Asset references those GUIDs (Scene→Mesh edges). Cook/Fast Path stay the load path.
- `gpu-driven-rendering`: Forest static MeshRenderers use the existing GPU-driven path when Meshlets exist. This slice SHALL NOT add MultiMesh Unique.

## Impact

- **Bake (apply later):** headless first-party baker in this repo; Godot source on the Windows machine (`SE-world.gltf`, `asset_index.json`, `assets/lib/**`, `assets/sets/{hub,fence,clearing,world}/**`). Writes DogWalk `Assets/Scenes/se-world.scene.asset` and Imports/cooks reachable SE/SL meshes there.
- **Runtime:** existing `SceneInstance::instantiate` + `GltfSceneImporter::attachEntityMeshes` / `importUnderEntity`. No `instance_asset_id` map at load. COL-* skipped for MeshRenderer.
- **Units:** engine-metre TRS in the Scene Asset; `similarityGltfToEngine` for glTF node/vertex basis. Do not apply Sponza centimetre scale to this forest.
- **Hosts:** editor Viewport and `engine_player` draw the DogWalk forest scene. Engine compiled default (`pick_test`) and Test/Sponza stay as they are.
- **Tests:** tiny synthetic `instance_asset_id` + `asset_index` fixtures under `engine/src/tests/` — not the 4795-instance Godot tree in this repo.
- **Docs:** CONTEXT flatten terms; [ADR 0071](../../../docs/adr/0071-se-world-flatten.md).
- **Does not wait on** PR 24 / `cursor/scene-collision-bridge-8a89`. Does not edit that branch or that PR.
- **This planning change** is OpenSpec artifacts + glossary/ADR only — no baker or GPU C++ until `/opsx:apply`.
