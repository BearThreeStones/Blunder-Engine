# Unique Mesh Assets stream; entities do not

Opening a Scene Asset still stalls the tick on unique Mesh CPU reads and `GpuMesh::create`. We decided on a **Mesh Loader**: **Jobs** read unique Mesh Assets (cooked `.meshbin` / Fast Path) into **Job data**; the RHI tick **budgets** `GpuMesh` uploads; draws **skip** a MeshRenderer until that GUID is GPU-resident. The entity table stays synchronous. Editor `openScene` and Play `engine_player` use the same `SceneSystem::loadScene` path. v1 does not stream per-instance glTF import, does not page entities, and does not rewrite Texture Loader. Domain: [CONTEXT.md — Mesh Loader](../../CONTEXT.md).

**Status:** proposed

## Considered Options

- **Keep blocking `loadMesh` + `getOrUploadGpuMesh` on open** — rejected. That is the hitch on 252 unique SE-world meshes.
- **Page entities / second PackedScene** — rejected. Grill: entity table complete on first Iterate.
- **Put `GpuMesh::create` inside a Job** — rejected. Jobs must not call RHI ([ADR 0057](0057-task-job-system.md)).
- **A second OS thread pool beside the Job System** — rejected. Complexity penalty; CPU work is Jobs.
- **Stream the per-instance glTF importer (~10k `openGltfImportDocument`)** — rejected. Grill: keep GUID Mesh bind; collision product exe is out.
- **Fold meshes into Texture Loader** — rejected. Missing `GpuMesh` skips the draw; missing texture uses Bindless fallback ([ADR 0058](0058-async-texture-loader.md)).
- **Loading-bar UI, frustum Job priority, binary Scene Asset, first-frame ms cap** — rejected this slice (Grill defaults).
- **Test/Sponza as the walk, or overwrite collision QC `root.scene.asset`** — rejected. Product file is DogWalk `se-world.scene.asset`.
- **Stack this planning git on flatten apply or `cursor/scene-collision-bridge-8a89`** — rejected. Branch from `main`.
