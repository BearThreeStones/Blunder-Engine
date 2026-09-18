# Tasks

## 1. Baker core (synthetic)

- [ ] 1.1 Add a headless flatten baker that reads `instance_asset_id` extras plus a stub `asset_index.json` and writes a flat Scene Asset (unique names `stem` / `stem_N`) and verify `se_world_flatten_test` (or equivalent) emits two layout entities from two extras and none from a node without extras
- [ ] 1.2 Skip missing asset ids and missing glTF files (do not abort) and verify the fixture with id `9c53197a476fa552` plus one valid neighbor writes only the valid instance
- [ ] 1.3 Expand nested library `instance_asset_id` under the layout entity and verify a tree-like fixture has child entities for the nested assets with shared Mesh Asset GUIDs
- [ ] 1.4 Apply `similarityGltfToEngine` to layout TRS, keep negative scale, and verify a translation with abs > 64 stays metres (not × `0.008`) and a source scale −1 is still negative

## 2. COL skip and Import contract

- [ ] 2.1 Skip MeshRenderer for `COL-*` nodes/entities and verify a set fixture with `COL-*` + `GEO-*` attaches only GEO
- [ ] 2.2 Keep runtime Import/attach ignorant of `instance_asset_id` (no `asset_index.json` lookup) and verify importing a glTF that still has extras does not spawn layout children and does not fail solely because extras exist
- [ ] 2.3 Point scene mesh fields at registered Mesh Asset GUIDs (shared per library asset) and verify two instances of one library id store the same GUID

## 3. DogWalk content bake (Windows apply)

- [ ] 3.1 Copy/Import reachable SE/SL library glTFs (`assets/lib/**`) and set glTFs (`assets/sets/{hub,fence,clearing,world}/**`) plus `.bin`/textures into `E:\Blunder Projects\DogWalk` and verify they register as Mesh Assets (ikea/zoo/vertical_slice not pulled)
- [ ] 3.2 Bake Godot `SE-world.gltf` into `Assets/Scenes/se-world.scene.asset` (~4795 layout instances minus skipped missing ids, set GEO present, Main Camera + Directional included) and verify `root.scene.asset` is byte-unchanged
- [ ] 3.3 Parent grouping uses entity `parent` names only (SE-world → set/hub groups → instances) and verify the Scene Asset has no `childScenes` payload

## 4. Open path and draw

- [ ] 4.1 When `--scene` and restore GUID are absent and `assets/Scenes/se-world.scene.asset` exists, open that scene (do not change engine `pick_test` default) and verify a DogWalk-style fixture opens se-world while the engine checkout still defaults to pick_test
- [ ] 4.2 Instantiate + `attachEntityMeshes` draws static forest MeshRenderers through existing GPU-driven rendering when Meshlets exist and verify no MultiMesh Unique type is added
- [ ] 4.3 Play (`engine_player`) with `--scene assets/Scenes/se-world.scene.asset` draws the forest without C# Find/group/ray and verify a Player load of that path succeeds in a headless or windowed smoke (human Viewport walk stays checklist)

## 5. Docs and validate

- [ ] 5.1 Keep `CONTEXT.md` flatten terms aligned with implementation names; ADR 0071 stays the bake-vs-runtime record
- [ ] 5.2 `openspec validate se-world-flatten --strict` passes
- [ ] 5.3 Do not edit `cursor/scene-collision-bridge-8a89` or PR 24; verify this change’s diff has no files from that branch
