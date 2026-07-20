# Task 5.3 — Reimport preserving GUID

Implement Reimport preserving GUID (from archived Source or Intermediate + settings).

## Requirements
1. Replace/extend `requestReimport` stub: if `archived_source` present and is FBX/OBJ, re-run Source Export over Intermediate (overwrite Intermediate glTF), keep same GUID, update descriptor if needed, invalidate Finals/dependents
2. If only Intermediate (no archived_source): re-apply import settings / touch Intermediate freshness + invalidate (no GUID change)
3. Preserve GUID always
4. Wire watch Source path to call this real Reimport (already calls requestReimport)

## TDD
RED then GREEN: import OBJ → capture GUID → modify archived Source or call Reimport → GUID unchanged, Intermediate refreshed and/or Finals stale.

Workdir: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-19-report.md
Mark 5.3. Do not push.
