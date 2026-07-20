# Task 2.1 — Descriptor archived_source field

Extend descriptor schema with optional archived Source path field; keep Intermediate `source` as Cook/Fast Path input.

## Field name (locked)
Use **`archived_source`** (optional string) on both Mesh and Texture Asset Descriptors. Empty/absent means no Source archive. Intermediate path remains **`source`**.

## TDD (mandatory)
1. RED: Add `asset_yaml_test` (or similar) under `engine/src/tests/` that:
   - Parses mesh YAML with `source` + `archived_source` and asserts both fields
   - Parses mesh YAML without `archived_source` (legacy) → empty archived_source, still succeeds
   - Round-trips serialize → parse for mesh and texture when archived_source is set
   - Omits `archived_source` key from serialized YAML when empty (keep descriptors clean)
2. Wire the test target in `engine/src/tests/CMakeLists.txt` like other `engine_runtime` tests
3. Configure/build in THIS worktree (`cmake --preset vs2026-debug` if needed; build dir `build/vs2026-debug`). Watch the test FAIL for the right reason before implementing.
4. GREEN: Minimal changes to `asset_descriptor.h` / `asset_yaml.cpp` (+ header) to pass
5. Re-run the test green; commit

## Constraints
- Do not implement Import/Source Export yet (later tasks)
- Do not rename Intermediate `source`
- Workdir: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
- Do not push; do not change git config
- Mark `- [x] 2.1` in openspec/changes/asset-pipeline-pull/tasks.md

## Report
Write e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull\.superpowers\sdd\task-3-report.md
(Note: task-3-report = OpenSpec task 2.1 / SDD sequence task 3)
Include: RED proof (failure output), GREEN proof (pass output), commits, status.
