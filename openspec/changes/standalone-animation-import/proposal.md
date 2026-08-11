## Why

DogWalk companion clips live in disconnected folders. Windows file pickers cannot multi-select across directories, so authors import Mesh and LOOP files separately. Today orphan companion glTFs are skipped, so standalone animation Import is impossible — a must-have for the real Chocomel workflow.

## What Changes

- Import companion-only glTF/GLB (animations ∧ meshes=0) as AnimationClip Assets **without** a Mesh host in the same batch.
- Stop treating orphan companions as hard-skip when Import Animations is enabled; register clips by companion file stem.
- Keep existing host+companion multi-select and near-disk pairing unchanged for Mesh Imports.
- Do **not** auto-fill AnimationPlayer maps or invent a Mesh host; clips remain independent GUID Assets.

## Capabilities

### New Capabilities
- `standalone-animation-import`: Companion-only glTF Import → AnimationClip Assets

### Modified Capabilities
- `asset-import`: Orphan companion paths become clip Import sources when animations are enabled (delta vs prior warn+skip-only behavior)

## Impact

- `AssetImportService::importExternalFiles` orphan handling
- `extractAndRegisterAnimationClipsFromGltf` (mesh_stem = companion stem)
- ADR 0021 / companion Import docs (orphan policy change)
- `asset_import_test` expectations for orphan-only batches
- Content Browser Import UX (same dialog; companion-only selections succeed)
