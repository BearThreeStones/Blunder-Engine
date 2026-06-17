---
name: blunder-engine
description: >-
  Blunder Engine project conventions, task routing, golden principles, and
  validation workflow. Use when editing engine/runtime/editor code, assets,
  CMake, Slint fork, or any multi-file engine work.
---

# Blunder Engine

C++20 game engine + editor. CMake, Vulkan offscreen render, Slint UI (fork), SDL3, Slang.

## Task routing

| Task | Read first | Also useful |
|------|------------|-------------|
| First time in repo | `docs/agents/overview.md`, `docs/golden-principles.md` | `structure.md`, `build.md` |
| Configure / build (Windows) | `docs/agents/build.md` | `CMakePresets.json`, `msvc-defines.md` |
| Build on Linux / Cursor Cloud | `docs/agents/cursor-cloud.md` | `build.md` |
| Change viewport / rendering | `docs/agents/render-pipeline.md` | `coordinate-system.md`, `slint-fork.md` |
| Change coordinates / glTF | `docs/agents/coordinate-system.md` | `CONTENT_LAYOUT.md` |
| Content Browser / assets / cook | `CONTENT_LAYOUT.md` | `structure.md` |
| Add CMake target or runtime system | `docs/agents/cmake.md` | `structure.md`, `golden-principles.md` |
| Slint submodule / UI renderer | `docs/agents/slint-fork.md` | `render-pipeline.md` |
| New feature (multi-file) | `docs/design-docs/architecture.md` | `docs/exec-plans/README.md` |
| Add tests (when introduced) | `docs/agents/testing.md` | `cmake.md` |

## Golden principles (summary)

1. **Z-up** world space; glTF Y-up converted at import.
2. **Virtual paths**: `assets/` descriptors, `resources/` raw files.
3. **Offscreen Vulkan + readback into Slint** — engine does not present 3D to HWND.
4. **Slint from fork** (`engine/3rdparty/slint`); rebuild `slint_cpp` after submodule changes.
5. **CMake presets** are build truth (`vs2026-debug`, `vs2026-release`).
6. **Match code style** before editing (`docs/agents/code-style.md`).
7. **New systems** via `engine/src/runtime/function/<name>/` + `RuntimeGlobalContext`.
8. **No fabricated test commands** — validate with build + `engine_editor`.
9. **Extend existing abstractions** (RHI, AssetManager, viewport sink).
10. **Re-cook** after import/mesh pipeline changes.
11. **Minimize scope** — smallest correct diff.
12. **Linux builds** follow `cursor-cloud.md`.
13. **MSVC defines** are centralized — do not redefine per-file.

Full list: `docs/golden-principles.md`.

## Default validation

1. `cmake --build build/vs2026-debug --config Debug --target engine_editor`
2. Run `engine_editor`; exercise affected UI/scene path.
3. If meshes/textures/import changed: re-cook per `CONTENT_LAYOUT.md`.

Or use slash command `/validate`.

## MCP setup (project: `.cursor/mcp.json`)

- **context7** — live docs for Vulkan, Slint, SDL3 (no API key).
- **github** — requires `GITHUB_PERSONAL_ACCESS_TOKEN` in your environment or `~/.cursor/mcp.json`.

## Code review checklist

- [ ] No edits under `engine/3rdparty/` except `engine/3rdparty/slint/`
- [ ] Z-up / virtual paths / offscreen render invariants respected
- [ ] Logging via `LOG_*` macros, not raw `printf`
- [ ] Namespace `Blunder`, `m_` member prefix, 2-space indent
- [ ] Build + run editor before claiming done
