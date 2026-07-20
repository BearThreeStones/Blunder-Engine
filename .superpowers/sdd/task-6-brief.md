# Task 2.3 — Scene serializer GUID Asset References

Scene serializer: read/write `guid`; mesh field as GUID Asset Reference; migrate legacy path→GUID on load and rewrite on save.

## Requirements
1. Scene JSON top-level `guid` field read/write through SceneSerializer (not only AssetRegistry splice)
2. Entity `mesh` field stores Mesh Asset GUID when saving
3. On load: if mesh is a legacy path (`assets/...mesh.yaml`), resolve via AssetRegistry::findGuidForPath when possible and treat as GUID; on save rewrite as GUID
4. Wire ensureSceneAssetRegistered (or equivalent) on scene load/save path used by editor if a clear hook exists — minimal, don't sprawl

## TDD mandatory
- Tests for: serialize/parse guid; save mesh as guid; load path mesh + registry maps to guid and save emits guid
- Prefer unit tests around SceneSerializer + registry without Vulkan

## Workdir
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull

## Report
e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull\.superpowers\sdd\task-6-report.md

Mark `- [x] 2.3` when done. Do not push.
