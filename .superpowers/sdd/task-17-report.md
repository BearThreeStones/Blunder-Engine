# Task 17 Report — OpenSpec 5.1 Import Intermediate terminology

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**OpenSpec:** `- [x] 5.1 Keep glTF/image Import as Intermediate register + Resources copy`

## Summary

- Renamed public helpers to Intermediate vocabulary: `isMeshIntermediateExtension` / `isTextureIntermediateExtension`; kept deprecated `isMeshSourceExtension` / `isTextureSourceExtension` aliases
- Import of glTF/images now copies Intermediate bodies under `Resources/Models/{name}/` or `Resources/Textures/{name}/` when the input is external or under `Resources/Source/`; already-non-Source Resources paths are reused
- Descriptor field `source` remains the Intermediate virtual path (`resources/...`); `archived_source` stays empty for glTF/PNG Import
- Logs and Slint callers say Intermediate (not “mesh source”); CONTENT_LAYOUT clarifies `source` = Intermediate path
- Did **not** implement Assimp Source Export (5.2)

## TDD evidence

- **RED:** `isMeshIntermediateExtension` / `isTextureIntermediateExtension` undeclared — C2039 (`task-17-red-excerpt.txt`)
- **GREEN:** `asset_import_test: all passed` (`task-17-green-run.txt`)

## Tests

`asset_import_test` — external glTF/PNG Import writes Assets descriptor + copies Intermediate under Resources (non-Source); `source` is `resources/...`; no `archived_source`

## Concerns

- glTF Import copies only the selected file (not sidecar `.bin` / textures); multi-file glTF packages still need follow-up if authors import from outside Resources
- Deprecated `isMeshSourceExtension` aliases remain for compatibility; prefer Intermediate names going forward
- Assimp FBX/OBJ dual-write is still 5.2
