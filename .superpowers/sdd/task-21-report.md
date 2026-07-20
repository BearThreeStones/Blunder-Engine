# Task 21 Report — OpenSpec 5.5 Import/Reimport tests gate

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Docs commits:** `a37d744`..`HEAD` (no new production code; coverage from 5.1–5.4; blend asserts in `aa8375f`)  
**OpenSpec:** `- [x] 5.5 Tests: FBX/OBJ Import dual-write; Reimport preserves GUID; glTF Import unchanged success path`

## Summary

Audited `asset_import_test` coverage for the three 5.5 themes. All themes already green from 5.1–5.3; no new production code. FBX runtime Assimp path documented as whitelist-only with OBJ dual-write evidence (no proprietary FBX fixture added).

## Coverage audit

| Theme | Evidence | Notes |
|-------|----------|-------|
| FBX/OBJ Import dual-write | `importObjSourceExportDualWritesArchiveAndIntermediate` | OBJ end-to-end: Source archive + Intermediate glTF + descriptor `archived_source` / GUID. FBX covered by `isMeshSourceExportExtension(".fbx")` whitelist assert; same Assimp export path as OBJ |
| Reimport preserves GUID | `reimportObjPreservesGuidAndRefreshesIntermediate`, `reimportIntermediateOnlyPreservesGuidAndInvalidatesFinal` | Archived-Source Assimp refresh + Intermediate-only invalidate; GUID / paths stable |
| glTF Import unchanged | `importMeshWritesIntermediateAndDescriptor` | Intermediate register + Resources copy; `archived_source` empty |

## Gaps considered / not added

1. **FBX binary fixture** — deferred; brief allows whitelist + OBJ Assimp evidence when proprietary FBX is hard.
2. **PNG / texture Reimport** — outside 5.5 mesh themes (invalidate-only already covered via Intermediate-only mesh case).

## Verification

Preset: `build/vs2026-debug` Debug. PATH includes `.cmake_deps/slint-build`.

| Target | Result |
|--------|--------|
| `asset_import_test` | exit 0 (`asset_import_test: all passed`) |

Shared log: `.superpowers/sdd/task-21-green-run.txt` (same run as task-20).

## OpenSpec tasks

- [x] 5.5 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns

1. Gate packages existing 5.1–5.3 tests; FBX Assimp bytes not exercised in CI.
2. Did not push.
