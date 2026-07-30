# Task 2.1 Report — Wire Companion Pairing into Import

## Status

COMPLETE

## Summary

- `AssetImportService::importExternalFiles` now classifies the selected glTF/GLB
  batch with `pairCompanionAnimationGltfMultiSelectBatch`.
- Accepted companion-only glTFs are removed from ordinary Mesh registration.
- With animations enabled, the unambiguous host Mesh Import receives the paired
  companion paths through `ImportResult::companion_animation_paths`.
- With animations disabled, the host still imports but companion association is
  empty and companion-only glTFs are skipped.
- Orphan companions are skipped and logged with `LOG_WARN` when animations are
  enabled.
- Multiple skinned hosts retain the helper's existing behavior: each imports as
  a Mesh with no companions, while ambiguous companions are warned and skipped.

This task deliberately does not copy companion Intermediate bodies or extract
their clips. Those remain Tasks 2.2 and 2.3.

## TDD Evidence

### RED

Added `importExternalFilesPairsCompanionsIntoMeshImport` to
`engine/src/tests/asset_import_test.cpp` before production changes.

Command:

```powershell
cmake --build build/vs2026-debug --config Debug --target asset_import_test
```

Expected failure observed:

```text
error C2039: "companion_animation_paths": is not a member of "Blunder::ImportResult"
```

The test covered:

- one host plus two companions produces one Mesh result;
- both companions reach the host Mesh Import handoff;
- companions are not registered as Mesh descriptors;
- disabling animations leaves the host companion handoff empty and still skips
  companion Mesh registration.

### GREEN

Commands:

```powershell
cmake --build build/vs2026-debug --config Debug --target asset_import_test
.\build\vs2026-debug\engine\src\tests\Debug\asset_import_test.exe
cmake --build build/vs2026-debug --config Debug --target engine_editor
```

Results:

- Build succeeded.
- `asset_import_test: all passed`.
- `engine_editor` build succeeded.

## Files Changed

- `engine/src/runtime/resource/asset_import/asset_import_service.h`
- `engine/src/runtime/resource/asset_import/asset_import_service.cpp`
- `engine/src/tests/asset_import_test.cpp`
- `openspec/changes/companion-animation-gltf-import/tasks.md`
- `.superpowers/sdd/task-2.1-report.md`

## Concerns / Follow-ups

- Companion paths are currently an in-memory handoff only. Task 2.2 must copy
  them under Resources and persist their association for Reimport.
- Task 2.3 must consume the persisted companion bodies through the existing
  clip extractor.
- Near-disk discovery remains intentionally unwired until Task 2.4.
- The build emits pre-existing MSVC PCH macro mismatch warnings for the test
  target; they did not fail the build or test.
