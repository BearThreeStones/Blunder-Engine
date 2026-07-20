# COLLADA as mesh Intermediate (not glTF)

Mesh Intermediate bodies are **COLLADA (`.dae`)**; Final remains platform cooked binaries under `.blunder/cooked/`. Khronos glTF/GLB are **Source Export inputs** (with FBX/OBJ), not Intermediate and not Final. Texture Intermediate stays image files; Mesh→Texture Asset References stay authoritative in descriptor `texture_guids`. Native `.dae` Import is Intermediate direct registration; existing glTF Intermediate upgrades lazily (GUID-preserving), and failed upgrades leave the old `source` intact. This supersedes the Intermediate=glTF / “avoid COLLADA” stance in [ADR 0012](0012-pull-asset-pipeline.md). Pull pipeline, GUID identity, YAML descriptors, and disk roots from 0012 still stand.

**See also:** [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline), [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md).
