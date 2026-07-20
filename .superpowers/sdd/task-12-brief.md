# Task 4.1 — Minimal Asset Dependency Graph

Build minimal Asset Dependency Graph (Scene→Mesh, Mesh→explicit Texture, Asset→Intermediate leaves).

## Requirements
1. Data structure + builder that, given FileSystem + AssetRegistry (and scene/descriptor reads):
   - Scene Asset → Mesh Asset GUIDs referenced
   - Mesh Asset → Texture Asset GUIDs when descriptor/import records explicit texture GUID refs (if no field yet, support optional YAML list e.g. `textures: [guid,...]` OR document that v1 mesh→texture edges are empty until import writes them — prefer add optional `texture_guids` array on mesh descriptor if cheap)
   - Each Asset → Intermediate leaf inputs (descriptor path + Intermediate source path) for freshness — can be side table
2. API to query dependents of a GUID (reverse edges) for later invalidation
3. TDD with temp fixtures

## Workdir
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-12-report.md
Mark 4.1. Do not push.
