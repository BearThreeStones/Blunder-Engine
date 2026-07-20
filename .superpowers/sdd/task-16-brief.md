# Task 4.5 — Watch/invalidation/Reimport tests gate

Tests: texture change invalidates mesh Final; descriptor change invalidates; Source change triggers Reimport hook.

## Scope
Audit existing tests from 4.1–4.4. Add missing cases with TDD if gaps:
1. Texture Intermediate/descriptor change → mesh Final stale (invalidate dependents)
2. Descriptor change → Final stale
3. SourceArchive change / archived_source map → Reimport hook invoked (may already exist)

Mark 4.5 when green evidence covers all three.

Workdir: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-16-report.md
Do not push.
