## Why

ADR 0013 replaces mesh Intermediate **glTF** with **COLLADA (`.dae`)**, matching Khronos authoring-interchange vs runtime-transmission roles while keeping Blunder Final as cooked binaries. The implemented Pull pipeline and `asset-pipeline-pull` specs still assume glTF Intermediate and Intermediate-direct glTF Import; those requirements must change before more content lands on the old format.

## What Changes

- **BREAKING:** Mesh Intermediate body format becomes COLLADA (`.dae`); Cook and Fast Path consume `.dae` (Assimp), not cgltf-primary Intermediate glTF
- **BREAKING:** `.gltf` / `.glb` Import becomes **Source Export** input (with FBX/OBJ): archive under Source root, export to `.dae` Intermediate — no longer Intermediate-direct registration
- Native `.dae` Import uses Intermediate-direct registration (former glTF role); images remain Intermediate-direct
- Source Export Assimp whitelist target: **FBX / OBJ / glTF / GLB → COLLADA**
- **Intermediate Upgrade:** lazy GUID-preserving conversion of existing glTF/GLB Intermediate `source` to `.dae` on project open / registry scan; on failure leave descriptor/`source` unchanged and allow temporary load of legacy glTF
- Mesh→Texture Asset References remain authoritative in descriptor `texture_guids` (COLLADA `<image>` URIs are not canonical)
- Docs already updated in grill (`CONTEXT.md`, `CONTENT_LAYOUT.md`, ADR 0012/0013); keep code and OpenSpec deltas aligned
- **Out of scope:** Final = glTF; Texture Asset as `.dae`; Collada2gltf packaging product; `.blend` auto-export; Material Asset nodes

## Capabilities

### New Capabilities
- _(none)_

### Modified Capabilities
- `asset-import`: Intermediate-direct formats, Source Export whitelist/target, Reimport Intermediate refresh
- `asset-pull-cook`: Fast Path / Cook Intermediate mesh input; freshness leaves; Intermediate Upgrade
- `asset-identity`: Intermediate mesh body language (COLLADA vs glTF); descriptor `source` expectations

## Impact

- Runtime: `AssetImportService` (extensions, Assimp export format), `AssetManager` / mesh cook path (Assimp COLLADA read; retire cgltf as Intermediate reader for new Assets), registry scan / upgrade hook, Content Browser Import filters
- On-disk: new Intermediate `.dae` under `Resources/Models/`; legacy `.gltf` Intermediate until upgraded; Source archive may hold glTF/GLB
- Specs: deltas against `asset-pipeline-pull` capabilities (`asset-import`, `asset-pull-cook`, `asset-identity`) — main `openspec/specs/` may still lack archived copies until that change archives
- Tests: Import dual-write to `.dae`; glTF Import archives + exports; `.dae` Intermediate-direct; Fast Path/Cook from `.dae`; Intermediate Upgrade success/failure; smoke suite updates
- Dependency: Assimp Collada exporter/importer (already in tree); cgltf may remain only for legacy Fast Path until upgrade succeeds
