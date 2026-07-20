# Golden Principles

> Subjective but enforceable rules for AI agents working on Blunder Engine.
> Each item links to the authoritative doc — do not duplicate long explanations here.

1. **World space is Z-up.** Right-handed engine space (+X right, +Y forward, +Z up). glTF sources are Y-up on disk; import applies a fixed basis change. Scene `.scene.asset` transforms are authored in engine space.  
   → [docs/agents/coordinate-system.md](agents/coordinate-system.md)

2. **Content uses virtual paths.** Descriptors live under `Assets/` (`assets/…`); Intermediate data under `Resources/` (`resources/…`); Source Assets under `Resources/Source/`. Built-in shaders are under `engine/shaders/`, not project content.  
   → [CONTENT_LAYOUT.md](../CONTENT_LAYOUT.md), [CONTEXT.md — Asset pipeline](../CONTEXT.md#asset-pipeline), [docs/adr/0012-pull-asset-pipeline.md](adr/0012-pull-asset-pipeline.md)

3. **The 3D viewport is offscreen Vulkan + readback into Slint.** The engine does not present the 3D scene to the HWND; `SlintSystem` + `SkiaRenderer` own window composition and Present. Do not assume Slint renders the scene directly.  
   → [docs/agents/render-pipeline.md](agents/render-pipeline.md)

4. **Slint comes from the fork submodule.** Branch `blunder/v1.16.1` on `BearThreeStones/slint`. Rebuild `slint_cpp` after submodule changes; follow fork maintenance steps.  
   → [docs/agents/slint-fork.md](agents/slint-fork.md)

5. **Build truth lives in CMake presets.** Use preset names and binary dirs from `CMakePresets.json` (`vs2026-debug`, `vs2026-release`). Documented commands in `build.md` must stay aligned.  
   → [CMakePresets.json](../CMakePresets.json), [docs/agents/build.md](agents/build.md)

6. **Match existing code style before editing.** Naming, includes, formatting, and namespace conventions are fixed — read them first.  
   → [docs/agents/code-style.md](agents/code-style.md)

7. **Follow CMake patterns for new systems.** Add sources under `engine/src/runtime/function/<name>/`, wire through `RuntimeGlobalContext`, update `engine/src/runtime/CMakeLists.txt`.  
   → [docs/agents/cmake.md](agents/cmake.md)

8. **Respect the conceptual vs physical architecture map.** README “five layers” are design labels; `runtime/core/layer/` is an application lifecycle pattern, not those layers.  
   → [docs/design-docs/architecture.md](design-docs/architecture.md)

9. **No fabricated test commands.** There is no first-party CTest suite yet. Validation is configure + build (+ run editor). When adding tests, use `engine/src/tests/` and CTest per `testing.md`.  
   → [docs/agents/testing.md](agents/testing.md)

10. **Prefer extending existing abstractions.** New render work goes through RHI / existing passes; new assets through `AssetManager` and cook pipeline; new UI through Slint + existing viewport sink. Avoid parallel one-off paths.

11. **Re-cook after import-axis or mesh pipeline changes.** Delete `.blunder/cooked/` or run `asset_compiler --force` so runtime does not serve stale `.meshbin` / `.texbin`.  
   → [CONTENT_LAYOUT.md](../CONTENT_LAYOUT.md)

12. **Versioned docs are the source of truth.** Invariants, architecture decisions, and task context belong in repo Markdown (`docs/`, `AGENTS.md`, `CONTENT_LAYOUT.md`). Do not rely on PR descriptions or chat as the only record.

13. **Minimize scope.** Smallest correct diff; do not refactor unrelated code or add speculative helpers.

14. **Linux/cloud builds use the documented alternate entry.** Symlink `CMakeLists_linux.cmake`, EASTL allocator flag, and Slint library paths differ from Windows — follow `cursor-cloud.md` instead of guessing.  
   → [docs/agents/cursor-cloud.md](agents/cursor-cloud.md)

15. **MSVC defines are centralized.** Do not redefine `NOMINMAX`, lean Windows headers, or CRT suppression per-file; use project-wide settings.  
   → [docs/agents/msvc-defines.md](agents/msvc-defines.md)
