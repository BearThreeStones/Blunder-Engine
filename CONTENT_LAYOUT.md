# Content layout (Assets / Resources)

Blunder separates **authoring configuration** from **raw source files**, similar in spirit to
Unity/Unreal workflows but with explicit on-disk roots.

## Roots

| Folder | Role | Virtual path prefix |
|--------|------|---------------------|
| [`Assets/`](Assets/) | Engine asset descriptors (materials, mesh wrappers, prefabs, scenes, project shaders) | `assets/` |
| [`Resources/`](Resources/) | Raw importer input (.gltf, .png, .wav, …) | `resources/` |
| [`engine/shaders/`](engine/shaders/) | Built-in engine Slang shaders (not project content) | *(none — `FileSystem::resolveShader`)* |

`FileSystem` resolves:

- `assets/Meshes/foo.mesh.asset` → `<project>/Assets/Meshes/foo.mesh.asset`
- `resources/Models/foo.gltf` → `<project>/Resources/Models/foo.gltf`
- `Models/foo.gltf` (no prefix) → Resources root (same as above)

## Mesh descriptor (`.mesh.asset`)

Minimal JSON wrapper that points at a Resources glTF:

```json
{
  "type": "Mesh",
  "source": "resources/Models/textured_cube/textured_cube.gltf"
}
```

`AssetManager::loadMesh("assets/Meshes/textured_cube.mesh.asset")` reads the descriptor and loads the `source` path.

## Scene descriptor (`.scene.asset`)

Static scene data (entity templates and nested child scene references). Runtime instantiation is handled by `SceneSystem::loadScene` (Stride-style `Scene` vs `SceneInstance` split).

```json
{
  "type": "Scene",
  "entities": [
    {
      "name": "Sun",
      "position": [0, 10, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    },
    {
      "name": "CameraRig",
      "parent": "Sun",
      "position": [0, 0, 5],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    }
  ],
  "childScenes": [
    {
      "scene": "assets/Scenes/sub.scene.asset",
      "name": "SubLevel",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "scale": [1, 1, 1]
    }
  ]
}
```

- `AssetManager::loadScene("assets/Scenes/root.scene.asset")` deserializes CPU-side `SceneAsset` data.
- `SceneSystem::loadScene` creates a `SceneInstance`, recursively loads `childScenes`, and sets `child.Parent` on the child instance (parent does not track children).
- Example startup scene: [`Assets/Scenes/root.scene.asset`](Assets/Scenes/root.scene.asset) with nested [`Assets/Scenes/sub.scene.asset`](Assets/Scenes/sub.scene.asset).

## glTF scene import (Sponza)

Large glTF scenes can be imported directly without a `.scene.asset` wrapper:

- Place Khronos [Sponza](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Sponza) under [`Resources/Models/Sponza/`](Resources/Models/Sponza/) (`Sponza.gltf` + external textures, no Draco).
- `SceneSystem::loadGltfScene("resources/Models/Sponza/Sponza.gltf")` parses the default scene, creates one entity per mesh primitive, and stores `MeshRendererComponent` data on the `SceneInstance`.
- `syncSceneToRender(RenderSystem*, SceneInstance*)` uploads meshes/textures and registers opaque/transparent draws (up to 256).
- `AssetManager::loadMeshPrimitive` caches each primitive as `{gltf_key}#mesh{m}#prim{p}`.

Editor startup loads Sponza when the files are present; see [`Resources/Models/Sponza/README.md`](Resources/Models/Sponza/README.md).

## Content browser data

`ContentIndex::scan(FileSystem)` recursively lists files under `Assets/` and `Resources/`
and returns [`ContentEntry`](engine/src/runtime/resource/content/content_entry.h) records
(virtual path, root, size, modified time). Slint UI will consume this later.

## Art source files

`Resources/Source/` is for non-runtime files (e.g. `.psd`). They are not scanned for loading.

## Thumbnail cache

Editor thumbnails are generated at startup by `ThumbnailGenerator` and stored under:

`<project>/.blunder/cache/thumbnails/`

| Item | Rule |
|------|------|
| Filename | `{sanitized_virtual_path}__{source_mtime_hex}.png` |
| Sidecar | Same stem with `.meta` JSON (`source_mtime`, `width`, `height`) |
| Invalidation | Regenerate when `ContentEntry::modified_time` differs from meta |
| Default size | 128×128 RGBA PNG |

Supported sources:

- **Images** (`.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`, `.ppm`): downscaled via `AssetManager::loadTexture2D`
- **Meshes** (`.gltf`, `.glb`, `.mesh.asset`): baseColor texture preview when available, otherwise a mesh placeholder icon
- **Directories**: folder placeholder icon
- **Other files**: generic file placeholder

The editor **Content Browser** (left dock, replaces Outliner) uses `ContentBrowserSystem` to scan content, show a folder tree + thumbnail grid, and drive `ThumbnailGenerator` cache paths.

| Interaction | Behavior |
|-------------|----------|
| **Refresh** | Rescans `Assets/` + `Resources/` and regenerates missing thumbnails |
| **Auto refresh** | efsw watches both roots; debounced re-scan (~300 ms) on external file changes (ignores `.blunder/`) |
| **Drag item → folder** | Moves a file within the same root (`assets/` or `resources/` only) |
| **OS file drop** | Drops onto the browser panel copy files into the selected folder |

Use `buildContentIndexWithThumbnails()` or `ContentBrowserSystem::refresh()` to populate `ContentEntry::thumbnail_cache_path`.
