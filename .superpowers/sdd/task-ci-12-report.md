# Task 4.3 Report — Docs match shipped COLLADA Intermediate

**Status:** DONE  
**Branch:** `feat/asset-pipeline-pull`  
**Worktree:** `e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull`  
**Commits:** `8ab6233`

## Spot-check (docs ↔ code)

| Claim | Doc | Code | Match |
|-------|-----|------|-------|
| Intermediate mesh = COLLADA `.dae` | CONTEXT / CONTENT_LAYOUT / ADR 0013 | `isMeshIntermediateExtension` → `.dae`; Assimp export `"collada"` | Yes |
| Source Export whitelist FBX/OBJ/glTF/GLB → `.dae` dual-write | CONTEXT / CONTENT_LAYOUT | `isMeshSourceExportExtension`; `importMeshSourceExport` + `archived_source` | Yes |
| `.dae` + images Intermediate-direct; `.blend` reject | CONTENT_LAYOUT Import table | import routing + clear reject message | Yes |
| Upgrade fail-soft (leave `source`, no partial `.dae`) | CONTEXT Intermediate Upgrade; ADR 0013 | `upgradeLegacyMeshIntermediates` fail path | Yes |
| Final = cooked `.blunder/cooked/{guid}.*bin` | All three | `loadCookedMeshAsset` / cook paths | Yes |
| Fast Path: Assimp COLLADA; legacy glTF via cgltf | CONTENT_LAYOUT mesh descriptor | `loadMeshFromColladaAssimp` + cgltf gate | Yes |

## Changes

- Confirmed working-tree doc updates already match shipped behavior — **no wording fixes required**
- Added/committed ADR 0013; synced CONTEXT / CONTENT_LAYOUT / ADR 0012 partial-supersession notes
- Marked `openspec/changes/collada-intermediate/tasks.md` `[x] 4.3`

## Concerns

- Sample/project Assets may still hold legacy glTF Intermediate until Upgrade runs (docs already describe this)
- Did not push
