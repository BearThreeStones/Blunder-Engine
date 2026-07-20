# Task 1.1 — Update CONTENT_LAYOUT.md

Update `CONTENT_LAYOUT.md` for three-tier roles, GUID Asset References, Scene `guid`, Pull/Fast Path, Source Export dual-write, and watch/Reimport.

## Required content (from grilled model / specs)

Three-tier:
- Source Asset → `Resources/Source/`
- Intermediate data → `Resources/` (non-Source); Intermediate descriptors → `Assets/`
- Final → `.blunder/cooked/{guid}.*bin`

Identity:
- Asset = GUID; Asset Descriptor YAML under Assets
- Asset Reference = GUID (scenes store mesh by GUID; path migrates)
- Scene `.scene.asset` includes `guid`, registered

Pull:
- On-demand Cook; Fast Path loads Intermediate when Final missing/stale
- Startup cookIfStale is optional warm-up only

Import:
- glTF/images = Intermediate register (do not call them Source)
- Source Export v1: FBX/OBJ → glTF via Assimp; dual-write archive under Source root
- Reimport preserves GUID; Source change auto-Reimport

Watch:
- Assets + Intermediate Resources; Source root triggers auto-Reimport

Link ADR 0012 and CONTEXT.md Asset pipeline section where natural (full cross-link is task 1.2 — light mentions OK here).

## Constraints
- Docs only for this task (no engine code)
- Keep existing useful detail (thumbnails, coordinate note) unless it contradicts the new model
- Workdir: e:\Dev\Blunder-Engine\.worktrees\asset-pipeline-pull

## Done when
CONTENT_LAYOUT.md reflects the model above; committed on feat/asset-pipeline-pull.
