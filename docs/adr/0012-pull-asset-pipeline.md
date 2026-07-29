# Pull Asset Pipeline with three-tier content

Authoring uses a **Pull** Asset Pipeline: Assets are GUID-identified units spanning Intermediate descriptors/data and regenerable Finals under `.blunder/cooked/`. Disk keeps `Assets/` (descriptors), `Resources/` (Intermediate bodies), `Resources/Source/` (Source archive), rather than a single Unity-style content tree. Rejected for v1: push-only full cook as the daily path; path-as-canonical references; silent `.blend` export; Material nodes in the dependency graph. Unreal-like on-demand derived data + Godot-like watch/reimport inform the loop; identity stays Unreal-like (GUID), not Godot path-primary.

**Partial supersession:** Mesh Intermediate format was briefly **COLLADA** ([ADR 0013](0013-collada-intermediate.md)); that is **superseded by [ADR 0019](0019-gltf-intermediate.md)** — mesh Intermediate is **glTF/GLB** again. Pull model, GUID identity, YAML descriptors, cooked Finals, and on-disk roots in this ADR remain in force.

**See also:** [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md) (on-disk roots, Import/Cook/watch), [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline) (vocabulary), [ADR 0019](0019-gltf-intermediate.md) (glTF Intermediate).
