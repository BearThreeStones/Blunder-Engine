# Task 8 Report — OpenSpec 2.5 Identity unit tests gate

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commit:** none (docs-only mark; no new production or test code)

## Summary

Audited identity coverage from tasks 2.1–2.4. All three 2.5 themes are already covered by green unit tests. No new cases added (TDD not triggered — no gaps).

## Coverage audit

| Theme | Evidence | Binary |
|-------|----------|--------|
| Registry GUID resolve | `rebuildFromScanRegistersSceneWithGuid`, `ensureUpgradesLegacySceneWithoutGuid`, `ensureRegistersExistingGuidWhenMissingFromRegistry`, `ensureRewritesInvalidGuidOnDisk` (`resolveGuid` / `findGuidForPath`); also `asset_manager_guid_test` (`resolveGuid` + `resolveGuidPath`) | `asset_registry_test`, `asset_manager_guid_test` |
| Scene guid upgrade | Legacy scene without guid → allocate+persist; register existing guid; rewrite invalid guid on disk; serializer round-trip of top-level `guid` | `asset_registry_test`, `scene_serializer_test` |
| Path→GUID mesh migration | `loadLegacyPathMeshMigratesToGuidOnSave` (path mesh → in-memory GUID; save emits GUID not path); soft-delete export preserves mesh GUID/path refs | `scene_serializer_test`, `scene_soft_delete_test` |

Supporting (2.1 schema, not a 2.5 theme gap): `asset_yaml_test` — `archived_source` parse/serialize/omit-empty.

## Gaps considered / not added

No real gaps for the three themes. Optional extras deferred (would not strengthen the gate):

1. Unknown GUID → empty `resolveGuid` alone (already covered via `loadMeshByGuid` unknown → null).
2. Mesh path migration without registry leaves path unchanged (documented in task-6 report; serializer behavior intentional).
3. Texture/scene `load*ByGuid` wrappers (thin resolve+path load; mesh path exercised without Vulkan).

## Verification (all exit 0)

Preset: `build/vs2026-debug` Debug. PATH includes Slint DLL dir for runtime-linked tests.

| Target | Result |
|--------|--------|
| `asset_yaml_test` | exit 0 |
| `asset_registry_test` | exit 0 (`asset_registry_test: all passed`) |
| `scene_serializer_test` | exit 0 (`scene_serializer_test: all passed`) |
| `asset_manager_guid_test` | exit 0 (`asset_manager_guid_test: all passed`) |
| `scene_soft_delete_test` | exit 0 (`scene_soft_delete_test: all passed`) |

Per-target logs: `.superpowers/sdd/task-8-*-build.txt`, `.superpowers/sdd/task-8-*-run.txt`

## OpenSpec tasks

- [x] 2.5 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns / notes

1. Gate is documentation + re-run evidence only; identity behavior lives in 2.1–2.4 commits.
2. Runtime-linked tests still need `slint_cpp.dll` (and typically `slang.dll`) on `PATH`.
3. Did not push.
