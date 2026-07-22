## Why

The current pipeline is half-push (Import + startup `cookIfStale`, Assets-only watch) and mixes vocabulary (glTF called “source”, path identity vs GUID). Authors need a Pull Asset Pipeline aligned with Source → Intermediate → Final so preview, Cook, and Reimport stay fast and coherent as content grows.

## What Changes

- Adopt **Source / Intermediate / Final** language and disk roles (`Resources/Source/`, `Resources/` data, `Assets/` descriptors, `.blunder/cooked/`)
- Make **Asset = GUID** the product identity; **Asset Reference** stores GUID (**BREAKING** for scene `mesh` path fields — migrate on load/save)
- Promote **Scene** to a first-class Asset (`.scene.asset` gains `guid`)
- Switch daily loop to **Pull**: on-demand Cook, Intermediate change invalidation, **Fast Path** (load Intermediate when Final missing/stale)
- Ship a **minimal Asset Dependency Graph** (Scene→Mesh, Mesh→Texture when explicit, Asset→own Intermediate inputs)
- Extend **Import** with **Source Export (v1)**: Assimp whitelist FBX/OBJ → glTF, **dual-write** archive under Source root; **Reimport**; watch Assets + Intermediate Resources; Source change auto-Reimport
- **Out of scope (v1):** `.blend`/`.psd` auto-export, Remote Control to DCC, Material Asset graph nodes, full packaging Manifest cull

## Capabilities

### New Capabilities
- `asset-identity`: GUID Asset, descriptors, content roots, Scene Asset, Asset Reference
- `asset-pull-cook`: Pull model, on-demand Cook, Fast Path, dependency graph, Asset Watch invalidation
- `asset-import`: Import, Source Export (Assimp FBX/OBJ), dual-write Source archive, Reimport

### Modified Capabilities
- _(none — no existing OpenSpec capability covers the content pipeline)_

## Impact

- Runtime: `AssetManager`, `AssetRegistry`, `AssetCompilerService`, `AssetImportService`, Content Browser watch, scene serializer / scene open path
- On-disk: descriptor YAML fields (`source` vs archived Source); `.scene.asset` schema; cooked meta may gain dependency stamps later
- Docs: `CONTEXT.md` (done in grill); `CONTENT_LAYOUT.md`; ADR for Pull + three-tier + GUID refs
- Tests: GUID resolve, stale/Fast Path load, dependency invalidation, FBX/OBJ export Import, Reimport, scene guid migration
- Dependency: Assimp used for Source Export write path (already in tree); Blender CLI not required for v1
