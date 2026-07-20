# Task 20 Report — OpenSpec 5.4 Reject `.blend` automatic export

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Feature commit:** `aa8375f`  
**OpenSpec:** `- [x] 5.4 Reject or clearly non-support `.blend` automatic export in v1 (copy-to-Source-only or error)`

## Summary

Audit gate for v1 `.blend` policy. Product behavior remains **clear reject** (not copy-to-Source-only, not silent success):

- `isMeshSourceExportExtension` whitelist is `.fbx` / `.obj` only — `.blend` is false
- `importMesh` falls through to `LOG_WARN` naming the FBX/OBJ whitelist and that `.blend` automatic export is not supported; returns `success=false` with empty guid / descriptor path
- `CONTENT_LAYOUT.md` Source Export section documents reject (no Asset / Intermediate / Source dual-write)
- No Assimp Blender importer path is invoked for Import

Kept current reject (brief preference). Did not add copy-to-Source-only.

## TDD / coverage

Existing `importUnsupportedSourceExportRejected` from 5.2 already asserted `!result.success`. Strengthened for 5.4 to also assert:

- empty `guid` and `descriptor_virtual_path`
- no `Assets/.../cube.mesh.yaml`
- no `Resources/Source/Models/cube` archive
- no Intermediate `Resources/Models/cube.gltf|glb`

Whitelist negative also covered in OBJ dual-write setup: `!isMeshSourceExportExtension(".blend")`.

## Verification

Preset: `build/vs2026-debug` Debug. PATH includes `.cmake_deps/slint-build` for `slint_cpp.dll`.

| Target | Result |
|--------|--------|
| `asset_import_test` | exit 0 — `asset_import_test: all passed` |

Evidence: `.superpowers/sdd/task-20-build.txt`, `.superpowers/sdd/task-20-green-run.txt` (includes `[warning] … unsupported mesh input …cube.blend`).

## Concerns

1. Reject is extension-based only; a renamed `.blend`→`.obj` would still hit Assimp (expected whitelist model).
2. `ImportResult` has no structured error string — UI must treat `success=false` + log warn as the user-visible failure for unsupported types.
3. Did not push.
