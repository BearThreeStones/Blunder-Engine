# SE-world flatten is a baked flat Scene Asset

Godot `SE-world` instances (`instance_asset_id` + `asset_index.json`) become **one flat Scene Asset** written by an **offline bake**. Runtime instantiate and mesh attach load that document. They **do not** look up `instance_asset_id`, do not keep a PackedScene/prefab graph, and do not revive `childScenes` ([ADR 0030](0030-remove-child-scenes.md)). Units stay **SI metres, Z-up** (`similarityGltfToEngine`); Sponza centimetre scale and the 64 m “looks like metres” heuristic are not applied to this forest. **COL-*** nodes get **no MeshRenderer** this slice and are **not spawned** as entities. Product file: DogWalk `Assets/Scenes/se-world.scene.asset`. Collision QC `root.scene.asset` is left alone. Test/Sponza are not the forest.

Domain: [CONTEXT.md](../../CONTEXT.md) (Scene Asset, SE-world flatten). GPU draw: [ADR 0070](0070-gpu-driven-rendering.md) — no MultiMesh Unique this slice.

**Status:** proposed

## Considered Options

- **Runtime `GltfSceneImporter` expansion of `instance_asset_id`** — rejected. Grill locked offline bake; load must not consult `asset_index.json`.
- **Nested Scene Assets / revived `childScenes`** — rejected. ADR 0030; `parent` is grouping only.
- **Placeholder boxes / empty instance nodes as the forest** — rejected. Layout instances are real library glTF.
- **Overwrite DogWalk `root.scene.asset`** — rejected. Collision QC for the scene collision bridge stays on that file.
- **Test or Sponza as the acceptance scene** — rejected. Product Project is DogWalk.
- **MultiMesh Unique for ~5k instances** — rejected. GPU-driven rendering already submits static MeshRenderers.
- **Draw COL-* or spawn `active: false` placeholders** — rejected this slice. COL-* nodes are omitted (no entity, no MeshRenderer). Next slice may bind static trimesh from COL or GEO.
- **Editor Import SE-world then Save as the shipping bake** — rejected. Production path is the offline baker writing one flat Scene Asset. Runtime must not look up `instance_asset_id`.
- **Wait on PR 24 / `cursor/scene-collision-bridge-8a89`** — rejected. Seeing the forest does not need the collision Unique land.
