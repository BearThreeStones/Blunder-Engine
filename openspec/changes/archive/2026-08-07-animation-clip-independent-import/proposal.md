## Why

Current Import still packages companion animations under the Mesh (`Models/{mesh}/companions/` + `companion_animation_sources`), which teaches authors that clips belong to a Mesh. Product law ([ADR 0028](../../../docs/adr/0031-animation-clip-independent-of-mesh.md)) says AnimationClip is independent — like Unreal Anim Sequences vs Skeletal Mesh. Docs already flipped; code must catch up so Task 1 / DogWalk Import stop producing Mesh-child Intermediate.

## What Changes

- **BREAKING:** Stop writing `companion_animation_sources` on Mesh descriptors; stop placing companion glTF Intermediate under `Models/{mesh}/companions/` or `Models/_standalone_companions/`.
- Place companion / animation-only glTF Intermediate under `Resources/Animations/<stem>/`; Clip descriptors own `source` for Reimport.
- Multi-select and near-disk remain Import *gestures* only: register independent Mesh and/or Clip Assets; same-batch bone mismatch against a skinned host warns but does not block; do not auto-fill AnimationPlayer maps.
- Mesh Reimport refreshes only Mesh; Clip Reimport refreshes only that Clip.
- Delete Mesh does not cascade Clips; Asset Dependency Graph has no Mesh→Clip packaging edge.
- One-shot migration moves legacy `companions/` and `_standalone_companions/` bodies into `Resources/Animations/<stem>/` and clears obsolete Mesh fields.

## Capabilities

### New Capabilities
- `animation-clip-independent-import`: Clip↔Mesh independence for Import Intermediate layout, packaging metadata removal, migration, and Reimport ownership

### Modified Capabilities
- `asset-import`: Companion and orphan paths no longer attach packaging state to Mesh; Intermediate layout and Reimport rules follow ADR 0028

## Impact

- `AssetImportService` (pairing, Intermediate copy, orphan/standalone paths, Reimport)
- Mesh YAML (`companion_animation_sources` field removal / ignore)
- `asset_import_test` and companion/standalone Import expectations
- Content Browser delete path (already non-cascade; verify no packaging-driven cascade)
- ADR 0021 packaging superseded by ADR 0028; companion / standalone OpenSpec changes remain historical context
- Existing Test Project Intermediate under `companions/` / `_standalone_companions/` needs migration tool or silent move on Import/open
