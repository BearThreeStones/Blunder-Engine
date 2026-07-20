# Task 2.4 — GUID-based load/resolve helpers

Add GUID-based load/resolve helpers on AssetManager / registry (path remains for migration/display).

## Requirements
1. Resolve GUID → descriptor virtual path via registry (already have resolveGuid)
2. AssetManager helpers to load mesh/texture/scene by GUID (resolve path then existing load by path)
3. SceneSystem temporary GUID→path bridge from 2.3 should use these helpers where appropriate (replace ad-hoc if present)
4. Path-based APIs remain

## TDD
- Test load-by-guid for mesh descriptor in temp project (or registry+manager as feasible without Vulkan)
- Fail first, then implement

## Workdir
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-7-report.md
Mark 2.4 done. Do not push.
