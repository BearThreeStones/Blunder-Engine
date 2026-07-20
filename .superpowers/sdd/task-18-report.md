# Task 18 Report — OpenSpec 5.2 Assimp Source Export dual-write

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Feature commit:** `dc95d61`  
**Docs commit:** `5c0a7d1`  
**OpenSpec:** `- [x] 5.2 Implement Assimp whitelist Source Export (FBX/OBJ→glTF), dual-write Source archive + Intermediate, descriptor fields`

## Summary

- Linked static Assimp privately into `engine_runtime`; lean Assimp CMake (tests/tools/samples/install off, static, warnings-as-errors off)
- `importMesh` dispatches: glTF/GLB → Intermediate register (5.1 unchanged); FBX/OBJ → Source Export dual-write
- Source Export: archive original under `Resources/Source/Models/{name}/`, Assimp Import + `gltf2` Export under `Resources/Models/{name}/`, descriptor `source` = Intermediate glTF, `archived_source` = Source archive path, GUID registered
- Whitelist via `isMeshSourceExportExtension` (`.fbx` / `.obj`); non-whitelist (e.g. `.blend`) rejected
- `importExternalFiles` routes FBX/OBJ through the same path

## TDD evidence

- **RED:** `isMeshSourceExportExtension` undeclared — C2039 (`task-18-red-excerpt.txt`)
- **GREEN:** `asset_import_test: all passed` (`task-18-green-run.txt`) — OBJ dual-write + blend reject + existing glTF/PNG paths

## Tests

`asset_import_test`:
- glTF/PNG Intermediate Import unchanged
- OBJ Source Export: archived under `Resources/Source`, Intermediate `.gltf` under `Resources/Models`, descriptor fields + GUID
- `.blend` rejected

## Concerns

- Assimp static Debug lib is large (~500MB); further importer trimming left for later if link/build time hurts
- OBJ→glTF may emit sidecar `.bin`; only the primary `.gltf` path is recorded as Intermediate `source`
- Full Reimport from archived Source still owned by 5.3
- FBX covered by whitelist API; runtime Assimp path exercised with OBJ fixture only
