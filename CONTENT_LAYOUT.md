# Content layout (Assets / Resources)

Blunder uses a **Pull** Asset Pipeline with three on-disk tiers.

| Role | Doc |
|------|-----|
| **On-disk layout & cook mechanics** (this file) | Roots, descriptors, Import, Watch, CLI |
| **Domain vocabulary** | [CONTEXT.md — Asset pipeline](CONTEXT.md#asset-pipeline) |
| **Decision records** | [ADR 0012](docs/adr/0012-pull-asset-pipeline.md) (Pull / three-tier), [ADR 0013](docs/adr/0013-collada-intermediate.md) (COLLADA mesh Intermediate) |

## Three-tier roles

| Tier | What it holds | On-disk root |
|------|---------------|--------------|
| **Source Asset** | DCC / exchange archives (e.g. `.fbx`, `.obj`, `.gltf`, `.glb`, `.psd`, `.blend`) | [`Resources/Source/`](Resources/Source/) |
| **Intermediate** | Descriptors (YAML / scene JSON) + readable data bodies (**COLLADA** `.dae`, images, …) | Descriptors → [`Assets/`](Assets/); data → [`Resources/`](Resources/) (non-Source) |
| **Final Asset** | Platform-optimized cooked binaries | `.blunder/cooked/{guid}.*bin` |

An **Asset** is the product unit of identity, keyed by **GUID**. One Asset binds an
**Asset Descriptor** under `Assets/` to Intermediate data under `Resources/` (and
optionally a Source Asset under `Resources/Source/`). Content Browser entries,
references, and Cook address Assets by GUID — not raw files alone.

## Roots

| Folder | Role | Virtual path prefix |
|--------|------|---------------------|
| [`Assets/`](Assets/) | Intermediate **descriptors** (YAML metadata, scenes) — Content Browser primary tree | `assets/` |
| [`Resources/`](Resources/) | Intermediate **data bodies** (.dae, .png, .wav, …); not the browser’s primary tree | `resources/` |
| [`Resources/Source/`](Resources/Source/) | **Source Assets** only — not loaded by the runtime, not Intermediate | *(under `resources/`)* |
| `.blunder/cooked/` | **Final Assets** keyed by GUID — derived, regenerable | *(cache, not a virtual content prefix)* |
| [`engine/shaders/`](engine/shaders/) | Built-in engine Slang shaders (not project content) | *(none — `FileSystem::resolveShader`)* |

`FileSystem` resolves:

- `assets/Meshes/foo.mesh.yaml` → `<project>/Assets/Meshes/foo.mesh.yaml`
- `resources/Models/foo.dae` → `<project>/Resources/Models/foo.dae`
- `Models/foo.dae` (no prefix) → Resources root (same as above)

Do **not** treat COLLADA/PNG under `Resources/` as Source Assets. Source is reserved for
DCC/exchange archives under `Resources/Source/` (including glTF/GLB after Source Export).
glTF/GLB under non-Source `Resources/` is a **legacy Intermediate** form pending
[Intermediate Upgrade](CONTEXT.md#asset-pipeline) to `.dae`.

## Identity and Asset References

- **Asset** = GUID. The Asset Descriptor YAML under `Assets/` persists `guid`, type,
  import settings, and references to Intermediate data (and optional archived Source).
- **Asset Reference** = the target Asset’s GUID. Scenes store mesh assignments as GUIDs;
  paths remain for display and migration. Legacy path-only scene fields resolve via the
  registry (`findGuidForPath`) and rewrite to GUID on save.
- **Scene Asset** (`.scene.asset`) includes a `guid` field, is registered in the Asset
  Registry like other Assets, and can act as a dependency-graph root.

## Mesh descriptor (`.mesh.yaml`)

YAML metadata (`type`, `guid`, `source`, `import`, optional `texture_guids`,
`archived_source`) pointing at Intermediate **COLLADA** (`.dae`) under Resources
(non-Source). The descriptor field `source` is the Intermediate data path (glossary),
not a Source Asset. `texture_guids` is the authoritative Mesh→Texture Asset Reference
list; COLLADA `<image>` URIs are interchange/preview only.

Load prefers a fresh Final under `.blunder/cooked/{guid}.meshbin`; otherwise **Fast Path**
loads Intermediate COLLADA (Assimp) and may request on-demand **Cook**. Legacy Intermediate
glTF `source` is upgraded lazily to `.dae` (see ADR 0013); on upgrade failure the old
`source` is left unchanged and may still load until a later success or Reimport.

Legacy JSON `.mesh.asset` files are still accepted with a migration warning.

## Texture descriptor (`.texture.yaml`)

YAML metadata (`type`, `guid`, `source`, `import`) pointing at an Intermediate image under
Resources. The descriptor field `source` is the Intermediate data path (glossary), not a
Source Asset. Load prefers `.blunder/cooked/{guid}.texbin` when fresh; otherwise Fast Path.

## Scene descriptor (`.scene.asset`)

Static scene data remains JSON for now (see existing scene docs). Documents include a
`guid` and are registered. Entity mesh links are Asset References (GUID).

Positions and `rotation` / `euler_degrees` use **engine world space** (right-handed
Z-up: +X right, +Y forward, +Z up). Intermediate COLLADA under `resources/` may arrive
Y-up from DCC tools; `AssetManager` / Cook convert at load (see `coordinate_system.h` in
[docs/agents/coordinate-system.md](docs/agents/coordinate-system.md)). After changing
import axes, force-recook affected Finals (`.meshbin`).

## GUID registry

`.blunder/asset_registry.yaml` maps `guid → assets/.../descriptor` (including Scene
Assets). Maintained by `AssetRegistry` on Import / Reimport / scene create-open-scan and
by cook tooling.

## Pull pipeline and Cook

Daily authoring is **Pull**: resolve Asset by GUID; use Final when present and fresh;
otherwise **Fast Path** (Intermediate → Loaded Asset) and **Cook** on demand. Full-project
Cook remains for packaging/CI.

Startup `AssetCompilerService::cookIfStale()` is an **optional warm-up**, not the
definition of freshness.

CLI: `asset_compiler --project-root <path> [--force]`

CMake target: `cook-assets`

Output cache:

```
.blunder/
  asset_registry.yaml
  cooked/
    {guid}.meshbin
    {guid}.meshbin.meta
    {guid}.texbin
    {guid}.texbin.meta
```

## Import and Source Export

| Action | Behavior |
|--------|----------|
| **Import Asset** button | Opens a file dialog; images and `.dae` register as Intermediate Assets; FBX/OBJ/glTF/GLB run Source Export first |
| **OS drag & drop** | Drop onto the Content Browser panel; same rules as the menu |
| **COLLADA / images** | Copy Intermediate data → `Resources/Models/{name}/` or `Resources/Textures/{name}/`; write descriptor → selected `Assets/` folder; allocate GUID and register. These are **not** Source Assets. |
| **Source Export (v1)** | Whitelist (Assimp): FBX/OBJ/glTF/GLB → Intermediate COLLADA (`.dae`) under `Resources/Models/{name}/`; **dual-write** archives the original under `Resources/Source/Models/{name}/`; descriptor `source` = Intermediate path, `archived_source` = Source archive path (`resources/Source/...`) |
| **Reimport** | Re-runs Import / Source Export for an existing Asset; **preserves GUID**; refreshes Intermediate; invalidates Finals and dependents. Distinct from Cook. |
| **Intermediate Upgrade** | On project open / registry scan: if mesh `source` is still glTF/GLB, convert to `.dae`, rewrite `source`, archive former glTF when needed, mark Finals stale. Failure leaves descriptor/`source` unchanged. |

`.blend` / `.psd` automatic Source Export is **out of v1**: Import returns
`success=false` (clear reject, no Asset / Intermediate / Source dual-write).
Files may still be placed under the Source root manually as archive-only.

## Asset Watch

Editor watching drives Pull freshness:

| Watched tree | On change |
|--------------|-----------|
| **Assets/** | Mark Finals (and dependents) stale; Content Browser re-scan |
| **Resources/** (non-Source Intermediate) | Same Final invalidation |
| **Resources/Source/** | Automatic **Reimport** for Assets that archive that Source path |

Debounce coalesces bursts; cooked cache and Import/Cook self-writes are not treated as
authoring edits.

## Thumbnail cache

Editor thumbnails are generated at startup by `ThumbnailGenerator` and stored under:

`<project>/.blunder/cache/thumbnails/`

Supported sources include `.mesh.yaml`, `.texture.yaml`, images, COLLADA (`.dae`), legacy
glTF Intermediate, and legacy `.mesh.asset`.

The editor **Content Browser** (left dock) uses `ContentBrowserSystem` to scan **Assets only**,
show a folder tree + thumbnail grid, and drive `ThumbnailGenerator` cache paths.

| Interaction | Behavior |
|-------------|----------|
| **Refresh** | Rescans `Assets/` and regenerates missing thumbnails |
| **Auto refresh** | efsw watches `Assets/` (and, for pipeline freshness, Intermediate Resources + Source — see Asset Watch); debounced re-scan (~300 ms) |
| **Drag item → folder** | Moves a file within `assets/` |
| **OS file drop** | Imports via `AssetImportService` (Intermediate under Resources + YAML descriptor; Source Export when applicable) |

## See also

- [CONTEXT.md — Asset pipeline](CONTEXT.md#asset-pipeline) — Source / Intermediate / Final, Asset, Pull, Cook, Fast Path, Import, Reimport
- [docs/adr/0012-pull-asset-pipeline.md](docs/adr/0012-pull-asset-pipeline.md) — why Pull + three-tier + GUID Asset References
- [docs/adr/0013-collada-intermediate.md](docs/adr/0013-collada-intermediate.md) — COLLADA mesh Intermediate (supersedes Intermediate=glTF)
- [docs/agents/structure.md](docs/agents/structure.md) — repo tree including `Assets/` / `Resources/`
- [docs/agents/common-tasks.md](docs/agents/common-tasks.md) — task routing for Content Browser / cook
