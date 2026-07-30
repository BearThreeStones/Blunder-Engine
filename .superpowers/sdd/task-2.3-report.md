# Task 2.3 Report — Extract Companion Animation Clips

## Status

COMPLETE

## Summary

- Mesh Import now extracts and registers clips from every copied companion
  Intermediate glTF after embedded mesh clips.
- Companion clip logical names prefer the companion filename stem. Files with
  multiple animations use the stem for the first clip and `_1`, `_2`, ...
  suffixes for later clips.
- Companion clips share the host mesh stem for their
  `resources/Animations/{mesh}/` Intermediate folder and are merged into the
  host `ImportResult::animation_clips`.
- Animation targets absent from the host skin emit a warning, while extraction
  and registration continue.

## TDD Evidence

The existing companion Import integration test was extended before production
changes to require:

- one host Mesh result plus all companion clip results;
- filename-stem naming for a single-animation companion;
- numeric suffix disambiguation for a multi-animation companion; and
- successful registration and retained track data for a `Tail` animation
  targeting a host whose skin only contains `Hips`.

The first run failed with 10 focused assertions because no companion clips were
registered. After implementation and correction of the synthetic fixture's
embedded buffer layout, the same test passed.

## Validation

Build:

```powershell
cmd /c "set CL=/Zm200&& cmake --build build/vs2026-debug --config Debug --target asset_import_test -- /m:1 /p:CL_MPCount=1"
```

Result: exit `0`.

Test:

```powershell
$env:PATH = "<worktree>/build/vs2026-debug/bin/Debug;<worktree>/.cmake_deps/slint-build;$env:VULKAN_SDK/Bin;$env:PATH"
.\build\vs2026-debug\engine\src\tests\Debug\asset_import_test.exe
```

Result: exit `0`, `asset_import_test: all passed`.

The passing output includes the expected mismatch warning:

```text
[AssetImport] companion animation bone 'Tail' is absent from host skeleton ...;
registering clip anyway (...)
```

## Files Changed

- `engine/src/runtime/resource/asset_import/asset_import_service.cpp`
- `engine/src/runtime/resource/asset_import/companion_animation_gltf.cpp`
- `engine/src/runtime/resource/asset_import/companion_animation_gltf.h`
- `engine/src/runtime/resource/asset_import/gltf_animation_clip_extractor.cpp`
- `engine/src/runtime/resource/asset_import/gltf_animation_clip_extractor.h`
- `engine/src/tests/asset_import_test.cpp`
- `openspec/changes/companion-animation-gltf-import/tasks.md`
- `.superpowers/sdd/task-2.3-report.md`

## Concerns / Follow-ups

- Task 3.1 still needs to refresh companion-derived clips from the persisted
  `companion_animation_sources` list while preserving GUIDs.
- Existing dirty submodules and `.superpowers/sdd/progress.md` are unrelated to
  Task 2.3 and are intentionally excluded from this commit.
