# Design: COL bake (slice A)

## Decision

Bake Godot `COL-*` mesh triangles into Scene Unique `ColliderComponent` (`triangleMesh`, Static). Do not draw COL. Do not use GEO as collision. Empty/unreadable triangles → skip (ADR 0057). Nodes whose name contains `detection` (case-insensitive) stay fall-through (Q8 triggers).

## Details

1. Flatten `visitNode`: COL → emit entity with collider triangles, no `mesh` GUID.
2. `cgltf_load_buffers` on flatten load so POSITION accessors are readable.
3. Vertices remapped with `transformPointGltfToEngine` (same as Mesh import).
4. Runtime `GltfSceneImporter` mirrors flatten for COL attach.
5. Product: rebake DogWalk `se-world.scene.asset` with `se_world_flatten`.

## Non-goals this PR

Sphere CCT, locomotion scripts, editor debug overlays.
