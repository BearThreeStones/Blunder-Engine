# Task 3.2 — Fast Path load + request Cook

Ensure load path: fresh Final → Final; else Fast Path Intermediate + request Cook (mesh and texture).

## Requirements
1. When loading mesh/texture by descriptor: if Final fresh, use Final (existing)
2. If Final missing/stale: Fast Path Intermediate (existing fallback) AND request `cookAsset(guid)` (sync is OK for v1; document)
3. Do not block returning the Fast Path Loaded Asset on Cook completion if Cook is slow — prefer: return Intermediate first, then call cookAsset (or cook after return). Brief preference: load Intermediate, then request Cook so next load gets Final.
4. Wire AssetManager to know about AssetCompilerService OR accept a callback/requestCook — minimal coupling preferred (weak_ptr or optional compiler pointer set from global_context)

## TDD
Prove that a load with missing Final still returns a mesh AND that cookAsset was requested (spy/counter or cooked file appears after load). RED then GREEN.

## Workdir
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-10-report.md
Mark 3.2. Do not push.
