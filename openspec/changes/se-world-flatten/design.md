# Design

## Context

See proposal.md for why. Grill locked a **flat Scene Asset**, **offline bake** (runtime does not look up `instance_asset_id`), **COL-* without MeshRenderer**, main file `Assets/Scenes/se-world.scene.asset`, and DogWalk (not Test/Sponza). Collision QC stays on `root.scene.asset`. Do not touch `cursor/scene-collision-bridge-8a89` or PR 24.

Today: `GltfSceneImporter` walks glTF nodes and primitives, applies `similarityGltfToEngine`, and can attach by entity name (`attachEntityMeshes` → `importUnderEntity`). It does not read `instance_asset_id`. `Scene` is a flat entity list ([ADR 0030](../../../docs/adr/0030-remove-child-scenes.md)). `findEntityByName` is a single map (last writer wins). `kMeterSpaceTranslationMaxAbs = 64` is a Sponza centimetre heuristic — forest layout translations already exceed that. GPU-driven rendering on `main` already draws static MeshRenderers without a 256 cap. Product scene is `E:\Blunder Projects\DogWalk`. Godot source: `E:\Godot Projects\dogwalk-repo\pro\game` (`gltf_import_script.gd` `process_instancing_node`). Decision record: [ADR 0071](../../../docs/adr/0071-se-world-flatten.md).

## Goals / Non-Goals

**Goals:**

- One headless baker that expands SE/SL `instance_asset_id` (layout + nested library) into `se-world.scene.asset` plus registered Mesh Assets in DogWalk.
- Runtime load is ordinary instantiate + mesh attach. Unique names. Skip missing ids. Metre Z-up TRS. COL-* not drawn.
- Viewport and Play show that scene. Engine tests use a tiny synthetic extras fixture.

**Non-Goals (design-level):**

- Runtime prefab / PackedScene / `instance_asset_id` table.
- Changing `GltfSceneImporter` into a Godot instancer on every Import.
- Binding Collider Unique / trimesh / CCT (PR 24).
- Rewriting GPU-driven rendering or adding MultiMesh Unique.
- Committing the Godot forest tree into Blunder-Engine.

## Decisions

1. **Offline bake writes the Scene Asset; runtime never consults `asset_index.json`.**  
   Apply ships a first-party headless baker (engine-repo C++, no mouse) that reads Godot extras the way `process_instancing_node` does, Imports reachable SE/SL glTFs into DogWalk, and writes one `.scene.asset`. Play/editor only load that JSON. The shipping path is that offline bake, not “editor Import SE-world then Save”. Runtime load must not look up `instance_asset_id`.  
   *Alternatives:* expand inside `GltfSceneImporter::importIntoScene` on every SE-world import (Grill: runtime must not look up `instance_asset_id`); keep PackedScene-like childScenes (forbidden by ADR 0030); treat editor Import + Save as the product bake (rejected).

2. **Shared Mesh Asset GUIDs, one entity per layout instance.**  
   196 library assets, ~4795 instance entities. Scene mesh fields store GUIDs ([asset-identity](../../../../openspec/specs/asset-identity/spec.md)). Instantiate creates the entity; `attachEntityMeshes` imports that library glTF under it (existing path).  
   *Alternatives:* duplicate a Mesh Asset per instance (disk blow-up); merge each instance into a unique flattened glTF (hides sharing, fights GPU-driven Meshlets).

3. **Nested library `instance_asset_id` become child entities under that layout instance.**  
   Tree glTFs still carry extras for needles/knots/leaves. Import ignores extras, so the baker must emit those children with their own mesh GUIDs or the tree stays a stub. Hierarchy row count MAY exceed ~5000; layout-instance count is the acceptance grain.  
   *Alternatives:* combine internals into one mesh at bake (loses author structure, extra cook work); hope `importUnderEntity` expands extras (it will not).

4. **COL-* are omitted this slice: no MeshRenderer and no entity.**  
   Skip glTF nodes whose display name starts with `COL-` in the baker and in runtime attach. GEO (and other non-COL visible meshes) draw. Do not spawn `COL-*` entities, including inactive (`active: false`) placeholders. Next knife may bind static trimesh from COL or GEO.  
   *Alternatives:* draw COL (z-fight with GEO); `active: false` placeholders (Grill locked: omit COL entities this slice).

5. **Unique names at bake, matching `makeUniqueEntityName`.**  
   `stem`, then `stem_1`… Godot repeats ~1196 names. Mesh attach and Authorship Address need uniqueness. Do not change `findEntityByName` into a multi-map this slice.  
   *Alternatives:* keep Godot duplicates and attach by graph order (breaks `attachEntityMeshes`); C# Find-by-path (out of scope).

6. **Skip missing ids; do not abort.**  
   `9c53197a476fa552` / `LI-bushlet_blue_delicate_006` × 8 is the known hole. Missing file = skip that instance, log, continue. Partial forest is still the product document.  
   *Alternatives:* hard-fail the bake (empty scene); substitute a box (Grill: real glTF, not placeholders).

7. **Engine-metre TRS in the Scene Asset; do not reuse the 64 m centimetre heuristic on this forest.**  
   Bake applies `similarityGltfToEngine` to node TRS. Vertex path stays the existing importer basis. Negative scale kept. `maybeAbsorbNodeScaleIntoAncestor` / `kGltfCentimeterToMeterScale` stay Sponza tools — they MUST NOT rescale SE-world layout. Forest translations already exceed `kMeterSpaceTranslationMaxAbs`.  
   *Alternatives:* author in glTF Y-up in the Scene Asset (fights golden principle 1); detect cm via 64 m max (mis-classifies the creek).

8. **Set GEO attach must not stamp COL or duplicate the whole set per GEO node.**  
   Prefer one grouping entity per set glTF (or set root) with a Mesh Asset GUID, attach once, skip COL-* MeshRenderers during that attach. Do not give every GEO primitive the same set-glTF GUID (that would import the whole set N times).  
   *Alternatives:* split every GEO primitive into its own glTF (more Import churn); keep COL as hidden meshes (`active: false`).

9. **Main scene file + open path; do not retarget engine Test.**  
   Write `assets/Scenes/se-world.scene.asset`. Leave `root.scene.asset` (collision QC) untouched. `--scene` still wins; Editor Session restore GUID still wins ([ADR 0046](../../../docs/adr/0046-editor-session-restore.md)). When both are absent and `se-world.scene.asset` exists in the Project, open it instead of falling through to `root.scene.asset`. Do not change engine-checkout `pick_test.scene.asset`. Do not add a `project.blunder` main-scene field. Include a New Scene–style Main Camera + Directional so Viewport/Play resolve a view; not 138 Spot.  
   *Alternatives:* overwrite `root.scene.asset` (clobbers PR 24 QC); change compiled `k_default_startup_scene_path` globally (breaks pick-test / engine checkout).

10. **Baker lives in this repo; forest bytes live in DogWalk.**  
    Engine tests: a few-node glTF with `instance_asset_id`, a stub `asset_index.json`, one missing id, one nested extras node, one `COL-*` / `GEO-*` pair, a negative scale. Human scene stays on the Windows DogWalk Project. Godot paths are apply-time inputs, not git LFS in Blunder-Engine.  
    *Alternatives:* Python bake outside the engine (TRS/basis drift); commit 5k entities into this repo.

11. **Draw path is existing GPU-driven; no MultiMesh Unique.**  
    Meshlet cook already happens on static meshes. This slice does not add a Godot MultiMesh analogue.  
    *Alternatives:* new instance buffer Unique (Grill: GPU-driven already extracts draws).

12. **Independent of the collision bridge implementation branch.**  
    No shared commits with `cursor/scene-collision-bridge-8a89`. No collider extras, groups, or CCT in this bake. Walker-on-forest is the next plan row.  
    *Alternatives:* wait for PR 24 to merge (not required to *see* the forest).

## Risks / Trade-offs

- **[Risk] `importUnderEntity` on a library glTF that still has extras creates empty child nodes** → Baker emits nested instances as scene entities; parent `mesh_virtual_path` only when that glTF has its own non-instance meshes; COL filter on attach.
- **[Risk] Centimetre heuristic fires on attach** → Forest TRS is already engine metres on the entity; do not run `looksLikeMeterSpaceTranslation` / 0.008 absorb on SE-world layout; tests cover a >64 m translation staying metres.
- **[Risk] Instantiate / attach cost at ~5k entities** → GPU-driven draw is not the 256 cap; first-open hitch is acceptable this slice; profiler is a later plan row.
- **[Risk] Godot source missing on Cloud** → Baker tests use synthetic fixtures; full bake is Windows apply with the Godot tree.
- **[Risk] Duplicate attach of set glTF** → Decision 8: one attach root per set file.
- **[Trade-off] Hierarchy rows > layout count** → Nested internals are required for complete trees; acceptance grain stays ~4795 layout instances.
- **[Trade-off] COL entities omitted** → Next collision knife may re-read set glTF for trimesh; this slice will not draw COL.

## Migration Plan

1. Land planning artifacts (this change). No baker yet.
2. On apply: synthetic baker tests, then Windows bake into DogWalk, Import/Cook meshes, write `se-world.scene.asset`, wire open-path fallback.
3. Rollback: delete or ignore `se-world.scene.asset`; `root.scene.asset` and Test/Sponza unchanged. Engine without the baker still opens other scenes.

No centimetre bake to migrate. No `childScenes` to revive.

## Open Questions

None that block specs. Godot extras key is `instance_asset_id` per Grill; confirm against `gltf_import_script.gd` at apply — behavior (bake extras, ignore at runtime) does not change if the exporter used an equivalent extras object.
