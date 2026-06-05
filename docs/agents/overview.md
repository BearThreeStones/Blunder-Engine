# Project Overview

> AI agent reference for the Blunder Engine codebase (a.k.a. ToyEngine).

C++20 game engine and editor using CMake, targeting **Windows** (Visual Studio 2026, v145) with an optional **Linux** path for cloud agents.

## Tech stack

| Area | Choice |
|------|--------|
| Language | C++20 |
| Build | CMake 4.0+ |
| Graphics | Vulkan (headless device + offscreen pass) |
| UI | Slint (**fork** submodule, Skia Vulkan/D3D12) |
| Windowing | SDL3 |
| Shaders | Slang |

**Dependencies:** git submodules under `engine/3rdparty/` (glm, spdlog, EASTL, SDL, Slint fork, efsw, assimp, cgltf, …).

## Submodule setup

```bash
git submodule update --init --recursive
# Slint fork (required for editor):
git submodule update --init engine/3rdparty/slint
```

## Three invariants (read before touching content or rendering)

1. **Z-up world space** — glTF is converted at import; scenes authored in engine space.  
   → [coordinate-system.md](coordinate-system.md)

2. **Virtual content paths** — `assets/` descriptors, `resources/` raw files.  
   → [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md)

3. **Offscreen 3D + Slint present** — engine renders to an offscreen target; pixels reach the UI via readback (or optional zero-copy).  
   → [render-pipeline.md](render-pipeline.md)

## Default validation

Configure and build with presets in [build.md](build.md), then run `engine_editor`. No first-party automated tests yet — see [testing.md](testing.md).

## Where to go next

| Need | Doc |
|------|-----|
| Task-based routing | [common-tasks.md](common-tasks.md) |
| Must-follow rules | [golden-principles.md](../golden-principles.md) |
| Full doc index | [AGENTS.md](../../AGENTS.md) |
| Architecture map | [design-docs/architecture.md](../design-docs/architecture.md) |

## See also

- [structure.md](structure.md)
- [build.md](build.md)
- [golden-principles.md](../golden-principles.md)
