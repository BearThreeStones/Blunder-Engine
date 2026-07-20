# Task 3 Report — OpenSpec 2.1 `archived_source`

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** `41de2ab` — feat: add optional archived_source to asset descriptors

## Summary

Added optional `archived_source` string to mesh and texture asset descriptors. Intermediate `source` is unchanged. Empty/absent `archived_source` means no Source archive; serialize omits the key when empty.

## TDD evidence

### RED (feature missing)

Test `asset_yaml_test` referenced `descriptor.archived_source` before the field existed.

Build failed with MSVC `C2039` (member not found), e.g.:

```
asset_yaml_test.cpp(36,20): error C2039: "archived_source": 不是 "Blunder::MeshAssetDescriptor" 的成员
asset_yaml_test.cpp(55,52): error C2039: "archived_source": 不是 "Blunder::MeshAssetDescriptor" 的成员
asset_yaml_test.cpp(63,6): error C2039: "archived_source": 不是 "Blunder::MeshAssetDescriptor" 的成员
asset_yaml_test.cpp(86,6): error C2039: "archived_source": 不是 "Blunder::TextureAssetDescriptor" 的成员
```

Full log: `.superpowers/sdd/task-3-red-build.txt`  
Excerpt: `.superpowers/sdd/task-3-red-excerpt.txt`

### GREEN (implementation)

Minimal production changes:

- `asset_descriptor.h` — `archived_source` on `MeshAssetDescriptor` and `TextureAssetDescriptor`
- `asset_yaml.cpp` — optional parse via `readOptionalStringField`; serialize only when non-empty
- `engine/src/tests/asset_yaml_test.cpp` + CMake wiring

Build: `cmake --build build/vs2026-debug --config Debug --target asset_yaml_test` → succeeded  
Run (with slang/slint DLLs on PATH): `asset_yaml_test.exe` → **exit 0**

Log: `.superpowers/sdd/task-3-green-build.txt`, `.superpowers/sdd/task-3-green-run.txt`

## Test coverage

| Case | Result |
|------|--------|
| Parse mesh with `source` + `archived_source` | pass |
| Parse legacy mesh without `archived_source` → empty | pass |
| Mesh round-trip serialize→parse | pass |
| Texture round-trip serialize→parse | pass |
| Omit empty `archived_source` from YAML | pass |

## OpenSpec tasks

- [x] 2.1 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. First configure in this worktree needed full submodule init; Slint required `blunder/v1.16.1` plus upstream tag `v1.16.1` for the ancestor check.
2. Local-only Slint UI sources (gitignored `slint/`) and uncommitted Slint fork patches (`mark_dirty_region` / `force_full_refresh`) had to be synced from the main checkout so `engine_runtime` could link; not part of this commit.
3. Runtime-linked tests need `slang.dll` and `slint_cpp.dll` on `PATH` (see `docs/agents/testing.md`).
4. Did not implement Import/Source Export (later tasks). Did not push.
