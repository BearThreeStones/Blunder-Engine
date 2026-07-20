# Task 3.1 — Formalize freshness / Cook API

Formalize freshness API: mark stale, cook one Asset, cook dependents; keep startup `cookIfStale` as optional warm-up only.

## Requirements
1. API on AssetCompilerService (or small collaborator) to:
   - markFinalStale(guid)
   - cookAsset(guid) — cook one
   - cookDependents / cookStaleDependents — can be stub that only cooks the one asset if dependency graph (4.x) not ready yet; document stub
2. Do not remove cookIfStale; frame it as warm-up
3. Prefer testable pure/freshness helpers if full cook is heavy

## TDD
- Unit tests for mark stale + cook-one invocation bookkeeping OR meta invalidation without full Vulkan if possible
- RED then GREEN

## Workdir
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-9-report.md
Mark 3.1. Do not push.

Look at existing AssetCompilerService::cookIfStale first.
