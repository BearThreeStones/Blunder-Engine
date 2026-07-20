# Task 4.3 — Watch Intermediate Resources

Extend file watch to Intermediate Resources (exclude Source for Intermediate invalidation; debounce; suppress self-writes).

## Requirements
1. ContentBrowserWatch (or new AssetWatch) also watches Resources root excluding Resources/Source
2. On Intermediate/descriptor change: map path → Asset GUID(s) → invalidateAssetAndDependents (need AssetCompilerService access)
3. Keep Assets watch for browser refresh
4. Debounce; suppress self-writes during Import/Cook (existing suppress pattern)
5. TDD: prefer unit-testable path→guid mapping / invalidate hook with fake events if efsw hard to test; integration-level OK if documented

Workdir: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-14-report.md
Mark 4.3. Do not push.
