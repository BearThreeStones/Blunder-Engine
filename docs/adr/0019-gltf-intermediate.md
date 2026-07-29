# glTF as mesh Intermediate (supersedes COLLADA)

Mesh Intermediate bodies are **glTF/GLB** again (including skinned meshes). **COLLADA (`.dae`) is removed** as mesh Intermediate. Final remains platform cooked binaries under `.blunder/cooked/` — glTF is not Final. Texture Intermediate stays image files; Mesh→Texture Asset References stay authoritative in descriptor `texture_guids`. Authored/exported `.gltf`/`.glb` Import as Intermediate (copy under `Resources/`); optional `.blend` (and other DCC) remain Source under `Resources/Source/` when archived. FBX/OBJ may Source-Export into glTF Intermediate when supported. Skeletal **AnimationClip** Intermediate stays readable **YAML sidecars** extracted at Import (Constant/Stepped preserved), not COLLADA channels and not “clips live only inside the mesh glTF with no Clip Asset.” This **supersedes [ADR 0013](0013-collada-intermediate.md)** (COLLADA Intermediate). Pull pipeline, GUID identity, YAML descriptors, and disk roots from [ADR 0012](0012-pull-asset-pipeline.md) still stand.

**Why:** DogWalk / Blender authoring is glTF-first; Assimp COLLADA round-trips risk skin weights and Stepped keys. Dual Intermediate formats were rejected; keeping COLLADA for “static only” was rejected as long-term debt. glTF-as-Final was rejected — Cooked binaries remain the runtime form.

**Migration:** Retire glTF→COLLADA Intermediate Upgrade. Remaining `.dae` Intermediate migrates GUID-preserving back to glTF or reimports from archived Source.

**See also:** [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline), [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md), animation Phase 1 terms in CONTEXT.
