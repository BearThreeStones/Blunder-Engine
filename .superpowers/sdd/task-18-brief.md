# Task 5.2 — Assimp Source Export FBX/OBJ→glTF dual-write

Implement Assimp whitelist Source Export (FBX/OBJ→glTF), dual-write Source archive + Intermediate, descriptor fields.

## Requirements
1. On Import of FBX/OBJ:
   - Copy original to `Resources/Source/...`
   - Convert to glTF Intermediate under `Resources/Models/...` via Assimp
   - Write Asset Descriptor with `source` = Intermediate glTF path, `archived_source` = Source archive path
   - Register GUID
2. Link Assimp into engine_runtime if not already (see engine/3rdparty/assimp)
3. glTF/image Import path from 5.1 unchanged
4. Whitelist: FBX, OBJ primarily; reject others for Source Export path

## TDD
RED then GREEN with a tiny OBJ fixture (commit a minimal .obj under tests/fixtures or generate in temp). Assert:
- archived_source under Resources/Source
- Intermediate .gltf exists under Resources
- descriptor fields set
- GUID registered

## Workdir
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull
Report: .superpowers/sdd/task-18-report.md
Mark 5.2. Do not push.

If Assimp export to glTF is painful, Assimp load + write a simple glTF via existing patterns is OK — prefer Assimp::Exporter with "gltf2" if available.
