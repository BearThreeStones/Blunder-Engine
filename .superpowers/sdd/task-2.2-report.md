# Task 2.2 Report — Persist Companion Intermediates

## Status

COMPLETE

## Summary

- Accepted companion glTF/GLB files are copied beneath the host mesh
  Intermediate at `resources/Models/{mesh}/companions/{filename}`.
- Companion-only files remain Resources data bodies and do not receive Mesh
  descriptors under `Assets/`.
- Mesh descriptors now persist the authoritative
  `companion_animation_sources` list for later Reimport work.
- `ImportResult::companion_animation_paths` now returns the persisted absolute
  Intermediate paths rather than the external input paths.
- Destination filename collisions receive a numeric suffix, and a failed
  companion batch removes copies already made by that batch.
- Clip extraction from companion bodies remains Task 2.3.

## TDD Coverage

The Task 2.1 integration test was extended first to require:

- copied companion contents under Resources;
- persisted paths returned from Mesh Import;
- no companion Mesh descriptors under Assets; and
- both virtual companion paths recorded in the parsed Mesh descriptor.

Descriptor YAML tests cover list round-trip and compatibility with descriptors
that omit `companion_animation_sources`.

## Validation

### RED

```powershell
cmake --build build/vs2026-debug --config Debug --target asset_yaml_test
```

The new descriptor test failed as expected because
`MeshAssetDescriptor::companion_animation_sources` did not exist.

### GREEN

The MSBuild targets were built sequentially to avoid racing their shared
runtime PCH:

```powershell
cmake --build build/vs2026-debug --config Debug --target asset_import_test -- /m:1
cmake --build build/vs2026-debug --config Debug --target asset_yaml_test -- /m:1
.\build\vs2026-debug\engine\src\tests\Debug\asset_yaml_test.exe
.\build\vs2026-debug\engine\src\tests\Debug\asset_import_test.exe
cmake --build build/vs2026-debug --config Debug --target engine_editor -- /m:1
```

Results:

- `asset_yaml_test` exited `0`.
- `asset_import_test: all passed`.
- `engine_editor` built successfully.

`git diff --check` passes (apart from the existing line-ending warning for
`.superpowers/sdd/progress.md`).

## Files Changed

- `CONTENT_LAYOUT.md`
- `engine/src/runtime/resource/asset/asset_descriptor.h`
- `engine/src/runtime/resource/asset/asset_yaml.cpp`
- `engine/src/runtime/resource/asset_import/asset_import_service.h`
- `engine/src/runtime/resource/asset_import/asset_import_service.cpp`
- `engine/src/tests/asset_import_test.cpp`
- `engine/src/tests/asset_yaml_test.cpp`
- `openspec/changes/companion-animation-gltf-import/tasks.md`
- `.superpowers/sdd/task-2.2-report.md`

## Concerns / Follow-ups

- Task 3.1 must consume `companion_animation_sources` when refreshing clips on
  Mesh Reimport.
- Requesting both test targets in one MSBuild invocation raced their shared
  runtime PCH/generated Slint objects; sequential `/m:1` builds passed.
- Existing dirty submodules and `.superpowers/sdd/progress.md` are unrelated to
  this task and are intentionally excluded from the commit.
