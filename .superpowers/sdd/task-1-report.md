# Task 1.1 Report — Update CONTENT_LAYOUT.md

## Status

**DONE**

## What changed

Updated `CONTENT_LAYOUT.md` only to document the Pull Asset Pipeline model:

- **Three-tier roles** — Source (`Resources/Source/`), Intermediate (descriptors in `Assets/`, data in `Resources/` non-Source), Final (`.blunder/cooked/{guid}.*bin`)
- **Identity** — Asset = GUID; Asset Descriptor YAML under Assets; Asset Reference = GUID (scene mesh fields; path migration); Scene `.scene.asset` includes `guid` and is registered
- **Pull / Fast Path** — on-demand Cook; Fast Path when Final missing/stale; startup `cookIfStale` framed as optional warm-up
- **Import / Source Export** — glTF/images = Intermediate register (not Source); Source Export v1 FBX/OBJ→glTF via Assimp with dual-write archive under Source root; Reimport preserves GUID
- **Asset Watch** — Assets + Intermediate Resources invalidate Finals; Source root triggers auto-Reimport
- Light cross-links to ADR 0012 and CONTEXT.md Asset pipeline section
- Preserved thumbnails, coordinate-system note, FileSystem resolve examples, cook CLI/cache layout

## Commit(s)

| Hash | Message |
|------|---------|
| `159ad2c6eb4b5d0dc7b250d6d5fc6658148ffaf8` | docs: document pull asset pipeline three-tier layout |

Branch: `feat/asset-pipeline-pull` (not pushed).

## Tests

N/A (docs-only).

## Self-review notes

- Checked every bullet in `task-1-brief.md` Required content against the rewritten doc; all covered with plan terminology (Source Asset, Intermediate, Final Asset, Asset Reference, Pull, Fast Path, Cook, Source Export, Reimport, Asset Watch).
- Avoided calling glTF/PNG “Source”; explicit warning kept under Roots.
- Did not invent a locked descriptor field name for archived Source (`source_asset` vs `archived_source` remains an open design question); doc says “optional archived Source path”.
- Left unrelated worktree dirt alone (`CMakeLists.txt` modified, `.superpowers/sdd/task-1-brief.md` untracked).
- Full cross-link polish deferred to task 1.2 as specified.

## Concerns

None.
