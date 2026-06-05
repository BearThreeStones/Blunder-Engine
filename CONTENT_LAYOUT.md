# Content layout (Assets / Resources)

Blunder separates **authoring configuration** from **raw source files**, similar in spirit to
Unity/Unreal/Stride workflows but with explicit on-disk roots.

## Roots

| Folder | Role | Virtual path prefix |
|--------|------|---------------------|
| [`Assets/`](Assets/) | Engine asset descriptors (YAML metadata, scenes) | `assets/` |
| [`Resources/`](Resources/) | Raw importer input (.gltf, .png, .wav, …) | `resources/` |
| [`engine/shaders/`](engine/shaders/) | Built-in engine Slang shaders (not project content) | *(none — `FileSystem::resolveShader`)* |

`FileSystem` resolves:

- `assets/Meshes/foo.mesh.yaml` → `<project>/Assets/Meshes/foo.mesh.yaml`
- `resources/Models/foo.gltf` → `<project>/Resources/Models/foo.gltf`
- `Models/foo.gltf` (no prefix) → Resources root (same as above)

## Mesh descriptor (`.mesh.yaml`)

YAML metadata pointing at a Resources glTF source (`type`, `guid`, `source`, `import`).

`AssetManager::loadMesh` reads the descriptor, prefers cooked data under
`.blunder/cooked/{guid}.meshbin`, and falls back to runtime cgltf import.

Legacy JSON `.mesh.asset` files are still accepted with a migration warning.

## Texture descriptor (`.texture.yaml`)

YAML metadata pointing at a Resources image (`type`, `guid`, `source`, `import`).

`AssetManager::loadTexture2D` prefers `.blunder/cooked/{guid}.texbin`.

## Scene descriptor (`.scene.asset`)

Static scene data remains JSON for now (see existing scene docs).

Positions and `rotation` / `euler_degrees` use **engine world space** (right-handed
Z-up: +X right, +Y forward, +Z up). glTF mesh sources under `resources/` stay
Y-up on disk; `AssetManager` converts them at import (see `coordinate_system.h` in
[docs/agents/coordinate-system.md](docs/agents/coordinate-system.md)). After changing import axes, force-recook affected `.meshbin` files.

## GUID registry

`.blunder/asset_registry.yaml` maps `guid → assets/.../descriptor.yaml`. Maintained by
`AssetRegistry` on import and by `asset_compiler` during cook.

## Import (Content Browser)

| Action | Behavior |
|--------|----------|
| **Import Asset** button | Opens a file dialog; images import immediately; glTF shows an import settings dialog |
| **OS drag & drop** | Drop onto the Content Browser panel; same rules as the menu |
| **Copy flow** | Source → `Resources/Models/{name}/` or `Resources/Textures/{name}/`; descriptor → selected `Assets/` folder |

## Asset compiler

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

The editor runs an incremental cook at startup (`AssetCompilerService::cookIfStale()`).

## Thumbnail cache

Editor thumbnails are generated at startup by `ThumbnailGenerator` and stored under:

`<project>/.blunder/cache/thumbnails/`

Supported sources include `.mesh.yaml`, `.texture.yaml`, images, glTF, and legacy `.mesh.asset`.

The editor **Content Browser** (left dock) uses `ContentBrowserSystem` to scan **Assets only**,
show a folder tree + thumbnail grid, and drive `ThumbnailGenerator` cache paths.

| Interaction | Behavior |
|-------------|----------|
| **Refresh** | Rescans `Assets/` and regenerates missing thumbnails |
| **Auto refresh** | efsw watches `Assets/`; debounced re-scan (~300 ms) |
| **Drag item → folder** | Moves a file within `assets/` |
| **OS file drop** | Imports via `AssetImportService` (Resources + YAML descriptor) |

## Art source files

`Resources/Source/` is for non-runtime files (e.g. `.psd`). They are not scanned for loading.
