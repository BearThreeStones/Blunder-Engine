# References

> Pointers to external docs and Blunder-specific entry points. Do not copy upstream manuals into this repo.

## Blunder agent guides (prefer these)

| Topic | Document |
|-------|----------|
| Windows build | [docs/agents/build.md](../agents/build.md) |
| Linux / Cursor Cloud | [docs/agents/cursor-cloud.md](../agents/cursor-cloud.md) |
| CMake presets | [CMakePresets.json](../../CMakePresets.json) |
| Content paths / Pull pipeline | [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md), [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline), [ADR 0012](../adr/0012-pull-asset-pipeline.md) |

## Toolchain (when you need upstream detail)

| Tool | URL | Use when |
|------|-----|----------|
| CMake | https://cmake.org/documentation/ | Presets, generators, `target_*` APIs |
| Vulkan SDK | https://vulkan.lunarg.com/ | SDK install, validation layers, `VULKAN_SDK` |
| Slang | https://github.com/shader-slang/slang | Shader language / SPIR-V (fetched by build) |
| Slint | https://slint.dev/docs | Upstream UI API (Blunder uses a **fork** — see [slint-fork.md](../agents/slint-fork.md)) |
| SDL3 | https://wiki.libsdl.org/SDL3/ | Windowing / events |
| Rust | https://doc.rust-lang.org/cargo/ | Required for Slint build (`rustc` / `cargo` 1.88+) |

## Content / interchange

| Library | URL | Use when |
|---------|-----|----------|
| glTF 2.0 | https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html | Authoring Resources meshes |
| cgltf | https://github.com/jkuhlmann/cgltf | Runtime glTF parse (header in `engine/3rdparty/cgltf`) |
| Assimp | https://assimp.org/ | Secondary import paths in 3rdparty |

## Submodule agent docs (vendor only)

These apply to **submodule** work, not Blunder first-party code:

| Path | Notes |
|------|-------|
| `engine/3rdparty/SDL/AGENTS.md` | SDL contribution guidelines |
| `engine/3rdparty/assimp/AGENTS.md` | Assimp agent guidelines |

## See also

- [docs/agents/common-tasks.md](../agents/common-tasks.md)
- [docs/golden-principles.md](../golden-principles.md)
