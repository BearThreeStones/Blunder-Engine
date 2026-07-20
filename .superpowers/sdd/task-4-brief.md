# Task 2.2 — Register Scene Assets in AssetRegistry

Register Scene documents in `AssetRegistry` (scan `*.scene.asset`); allocate/persist `guid` on create/open/upgrade.

## Scope (this task only)
- `rebuildFromScan` includes `*.scene.asset` under Assets when they have a valid `guid` field (JSON)
- Provide a helper (e.g. `ensureSceneAssetRegistered` / `upgradeSceneGuidIfNeeded`) that:
  - If scene JSON lacks `guid`: allocate GUID, write it into the document on disk, register
  - If scene has guid: register if missing from registry
- Do NOT yet change mesh field to GUID (that is task 2.3)

## TDD mandatory
1. RED: `asset_registry_test` (temp project root via FileSystem) asserting:
   - rebuildFromScan registers a `.scene.asset` that already contains guid
   - ensure/upgrade allocates guid for a legacy scene without guid, persists file, registry resolves
2. Watch fail correctly → GREEN minimal code → pass → commit
3. Mark `- [x] 2.2` in tasks.md

## Workdir
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull

## Report
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull\.superpowers\sdd\task-4-report.md
Include RED/GREEN evidence.

Do not push. Do not change git config.
