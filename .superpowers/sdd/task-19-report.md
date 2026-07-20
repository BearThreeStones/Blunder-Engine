# Task 19 Report — OpenSpec 5.3 Reimport preserving GUID

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Feature commit:** `2c2c481`  
**Docs/evidence commit:** `b1f6b54`  
**OpenSpec:** `- [x] 5.3 Implement Reimport preserving GUID (from archived Source or Intermediate + settings)`

## Summary

- Replaced `requestReimport` / `requestReimports` stub with real Reimport:
  - **FBX/OBJ + `archived_source`:** Assimp re-export overwrites Intermediate glTF at the existing `source` path; GUID / descriptor / registry unchanged; Finals invalidated
  - **Intermediate-only (no archived Source):** GUID preserved; Finals invalidated (settings refresh optional / not required)
- Refactored Assimp helpers (`readSourceScene`, `exportSceneToGltfFile`, `reexportArchivedSourceToIntermediate`) so Import (5.2) and Reimport (5.3) share export
- On Assimp / missing-Source failure: log warn, **still invalidate Finals** (watch fixtures with non-Assimp placeholders keep working)
- Watch Source path already called `requestReimports`; no watch wiring change needed

## TDD evidence

- **RED:** stamped Intermediate marker survives stub Reimport — `FAIL Reimport refreshes Intermediate (not stale marker)` (`task-19-red-excerpt.txt`; full stub log in `task-19-red-run.txt`)
- **GREEN:** `asset_import_test: all passed` (`task-19-green-run.txt`); `asset_watch_path_test: all passed`

## Tests

`asset_import_test`:
- Existing glTF/PNG Intermediate Import + OBJ Source Export dual-write unchanged
- `reimportObjPreservesGuidAndRefreshesIntermediate` — import OBJ → GUID → modify archived Source → Reimport → same GUID, Intermediate overwritten, Finals gone
- `reimportIntermediateOnlyPreservesGuidAndInvalidatesFinal` — glTF Import → Reimport → same GUID, Finals gone

`asset_watch_path_test`: Source→`requestReimports` path still green (invalidate even when Assimp cannot read placeholder `.fbx`)

## Concerns

- Assimp re-export failure leaves Intermediate untouched but Finals stale (intentional; next cook may Fast Path Intermediate)
- Watch-path batch fixtures use placeholder FBX bytes → expected Assimp warn + invalidate-only for those GUIDs
- Optional “refresh import settings on Intermediate-only Reimport” not implemented (invalidate-only is enough for v1)
- Task 5.5 still owns broader dual-write / Reimport / glTF regression gate packaging
