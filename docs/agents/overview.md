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

2. **Virtual content paths** — `assets/` descriptors, `resources/` Intermediate data (not Source).  
   → [CONTENT_LAYOUT.md](../../CONTENT_LAYOUT.md), [CONTEXT.md — Asset pipeline](../../CONTEXT.md#asset-pipeline), [ADR 0012](../adr/0012-pull-asset-pipeline.md)

3. **Offscreen 3D + Slint present** — engine renders to an offscreen target; pixels reach the UI via readback (or optional zero-copy).  
   → [render-pipeline.md](render-pipeline.md)

## Default validation

Configure and build with presets in [build.md](build.md). Run relevant first-party tests per [testing.md](testing.md). If no existing test name matches the edited files, an `engine_editor` build is the documented fallback. Re-cook when import or mesh/texture pipeline changed. See [common-tasks.md](common-tasks.md#default-validation). Merge CI is the GitHub Actions Linux job at pull request / `main`; it is not session Completion evidence.

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
