# Task 21 Report — OpenSpec 5.5 Import/Reimport tests gate

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**OpenSpec:** `- [x] 5.5 Tests: FBX/OBJ Import dual-write; Reimport preserves GUID; glTF Import unchanged success path`  
**Commit:** docs/gate only (no new production code; coverage from 5.1–5.4)

## Summary

Audited `asset_import_test` against the three 5.5 themes. All required behaviors are already covered and green. No new fixtures added.

## Coverage audit

| Theme | Evidence | Notes |
|-------|----------|--------|
| OBJ Import dual-write | `importObjSourceExportDualWritesArchiveAndIntermediate` | Source archive under `Resources/Source`, Intermediate `.gltf` under `Resources/Models`, descriptor `source` + `archived_source` + GUID |
| FBX Import dual-write | `isMeshSourceExportExtension(".fbx")` true in same test; same Assimp path as OBJ | **Whitelist-only** — no proprietary FBX binary fixture (brief allows OBJ as Assimp evidence) |
| Reimport preserves GUID | `reimportObjPreservesGuidAndRefreshesIntermediate`; `reimportIntermediateOnlyPreservesGuidAndInvalidatesFinal` | OBJ: refresh Intermediate from archived Source; glTF Intermediate-only: invalidate Finals |
| glTF Import unchanged | `importMeshWritesIntermediateAndDescriptor` | Intermediate under Resources (non-Source), empty `archived_source`, registry GUID |

Supporting: PNG Intermediate Import; `.blend` reject (5.4).

## Gaps considered / not added

1. **FBX binary dual-write fixture** — deferred; Assimp Source Export path is exercised end-to-end via OBJ; FBX is whitelist-gated. Adding a tiny ASCII FBX is fragile / not worth proprietary binary.
2. No production gaps for this gate.

## Verification

Same green run as task 5.4 (full suite):

| Target | Result |
|--------|--------|
| `asset_import_test` | exit 0 — `asset_import_test: all passed` |

Evidence: `.superpowers/sdd/task-21-green-run.txt` (copy of task-20 green run).

## Concerns

1. FBX runtime dual-write is API/whitelist + shared Assimp codepath confidence, not a separate binary fixture.
2. Watch-path placeholder FBX bytes still expect Assimp warn + invalidate-only (see task 5.3 report) — outside this gate’s Import dual-write claim.
3. Did not push.
