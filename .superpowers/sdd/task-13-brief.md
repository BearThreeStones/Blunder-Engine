# Task 4.2 — Propagate Final invalidation

Propagate Final invalidation along reverse edges of the Asset Dependency Graph.

## Requirements
1. Given a changed Asset GUID (or Intermediate leaf), markFinalStale on that asset and all dependentsOf(guid) via AssetCompilerService
2. Provide a helper e.g. invalidateAssetAndDependents(guid) that uses the graph + markFinalStale
3. cookDependents can call cookAsset on dependents (optional upgrade from stub)
4. TDD with graph fixtures

Workdir: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-13-report.md
Mark 4.2. Do not push.
