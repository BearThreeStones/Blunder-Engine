# Task 20 Report — OpenSpec 5.4 Reject `.blend` automatic export

**Status:** done  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Feature commit:** `aa8375f`  
**Docs commits:** `a37d744`..`HEAD`  
**OpenSpec:** `- [x] 5.4 Reject or clearly non-support .blend automatic export in v1 (copy-to-Source-only or error)`

## Summary

Audited v1 `.blend` behavior from 5.2. Product choice: **clear reject** (`success=false`), not copy-to-Source-only and not silent success. Coverage already existed; strengthened assertions + clearer warn / docs.

## Product behavior

| Path | Behavior |
|------|----------|
| `importMesh("*.blend")` | Not Intermediate, not Source Export whitelist → `ImportResult.success=false`, empty guid/path |
| Side effects | No Assets descriptor, no `Resources/Source` archive, no Intermediate under Models |
| Log | `LOG_WARN` names FBX/OBJ whitelist and states `.blend` automatic export is not supported |
| Docs | `CONTENT_LAYOUT.md` states Import returns `success=false` (manual Source archive-only still allowed) |

Copy-to-Source-only was considered and **rejected** per brief preference to keep the error path.

## TDD / evidence

- Pre-existing: `importUnsupportedSourceExportRejected` asserted `!result.success` (from 5.2)
- Strengthened: whitelist exclusion + no silent dual-write side effects
- Warn string clarified for operators
- **GREEN:** `asset_import_test: all passed` (`task-20-green-run.txt`); build (`task-20-build.txt`)

## Tests

`asset_import_test` → `importUnsupportedSourceExportRejected`:
- `.blend` ∉ Source Export / Intermediate extensions
- `success=false`, empty guid + descriptor path
- no Assets / Source / Intermediate writes

## OpenSpec tasks

- [x] 5.4 in `openspec/changes/asset-pipeline-pull/tasks.md`

## Concerns

1. `importExternalFiles` still silently skips non-whitelist mesh extensions (no result entry); direct `importMesh` is the clear-error API.
2. UI may still need to surface the warn to the user (log-only today).
3. Did not push.
