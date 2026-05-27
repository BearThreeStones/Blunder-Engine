# AGENTS.md - Blunder Engine

> AI agent reference for the Blunder Engine codebase (a.k.a. ToyEngine).

## Project Overview

C++20 game engine using CMake, targeting Windows with Visual Studio 2026.

**Tech Stack:** C++20 | CMake 4.0+ | Vulkan | VS 2026 (v145)

**Dependencies (git submodules):** glm, spdlog, EASTL, SDL, Slint (**fork**), efsw, …

---

## Build Commands

```bash
# Configure (use Developer PowerShell for VS 2026 for bundled CMake 4.x)
cmake --preset vs2026-debug
cmake --preset vs2026-release

# Build Debug / Release
cmake --build build/vs2026-debug --config Debug
cmake --build build/vs2026-release --config Release

# Or use build presets
cmake --build --preset vs2026-debug
cmake --build --preset vs2026-release

# Build specific target
cmake --build build/vs2026-debug --config Debug --target engine_editor

# After changing engine/3rdparty/slint (fork branch), rebuild Slint before the editor:
cmake --build build/vs2026-debug --config Debug --target slint_cpp
cmake --build build/vs2026-debug --config Debug --target engine_editor

# Open in Visual Studio
start build/vs2026-debug/BlunderEngine.sln

# Run editor
./build/vs2026-debug/engine/src/editor/Debug/engine_editor.exe
```

---

## Testing

> **Note:** No test infrastructure yet. When adding tests, use CTest and create `engine/src/tests/`.

---

## Directory Structure

```
project/
├── Assets/                # Engine asset config (virtual paths: assets/...)
├── Resources/             # Raw source files (virtual paths: resources/...)
├── engine/
│   ├── src/
│   │   ├── runtime/       # Core library (engine_runtime)
│   │   └── editor/        # Editor executable (engine_editor)
│   └── shaders/           # Built-in Slang (not under Assets/)
└── engine/3rdparty/       # Git submodules (under engine/)
```

See [CONTENT_LAYOUT.md](CONTENT_LAYOUT.md) for virtual paths, `.mesh.asset` descriptors,
`ContentIndex`, the `.blunder/cache/thumbnails/` thumbnail cache (`ThumbnailGenerator`), and the Slint **Content Browser** (`ContentBrowserSystem` in `resource/content_browser/`).

**efsw** ([SpartanJ/efsw](https://github.com/SpartanJ/efsw)) is vendored at `engine/3rdparty/efsw`
for cross-platform directory/file change notifications. `ContentBrowserWatch` uses it to
auto-refresh the Content Browser when `Assets/` or `Resources/` change on disk (debounced;
skips `.blunder/`). Init submodule: `git submodule update --init engine/3rdparty/efsw`.
Defines `BLUNDER_HAS_EFSW` and links `efsw` when the target exists.

---

## Code Style

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `LogSystem`, `InputSystem` |
| Methods | camelCase/PascalCase | `initialize()`, `Tick()` |
| Member vars | m_prefix | `m_logger`, `m_game_command` |
| Local vars | snake_case | `console_sink` |
| Constants | k_prefix | `k_complement_control_command` |
| Enums | PascalCase class, lowercase values | `LogLevel::debug` |
| Macros | SCREAMING_CASE | `LOG_DEBUG`, `ASSERT` |
| Template params | ALL_CAPS | `TARGS` |
| Namespace | `Blunder` | All engine code |

### Header Files

- Use `#pragma once`
- Include order: corresponding header → project → third-party → standard
- Use full paths from `runtime/` root: `#include "runtime/core/log/log_system.h"`

### Formatting

- 2-space indentation (no tabs)
- Opening brace on same line
- Use `final` for non-inheritable classes
- Close namespaces with comment: `}  // namespace Blunder`

```cpp
class LogSystem final {
 public:
  LogSystem();
  
 private:
  std::shared_ptr<spdlog::logger> m_logger;
};
```

### Logging

```cpp
LOG_DEBUG("Debug: {}", value);
LOG_INFO("Info message");
LOG_WARN("Warning: {}", warning);
LOG_ERROR("Error: {}", error);
LOG_FATAL("Fatal - throws: {}", msg);
```

### Memory Management

- `std::shared_ptr` for system singletons
- Global context: `g_runtime_global_context`
- Reset in reverse order during shutdown

### Error Handling

- `LOG_FATAL` throws `std::runtime_error`
- `ASSERT()` for debug-only checks (disabled in NDEBUG)

---

## CMake Conventions

### Adding Source Files

```cmake
add_library(engine_runtime
    engine.cpp
    "function/input/input_system.cpp"
)
```

### Linking

- `PUBLIC`: Dependencies in public headers
- `PRIVATE`: Internal implementation only

```cmake
target_link_libraries(engine_runtime
    PUBLIC glm spdlog
    PRIVATE glfw
)
```

---

## Adding a New System

1. Create header/source in `engine/src/runtime/function/<system>/`
2. Add `std::shared_ptr<YourSystem>` to `RuntimeGlobalContext`
3. Initialize in `startSystems()`, cleanup in `shutdownSystems()`
4. Add files to `engine/src/runtime/CMakeLists.txt`

---

## Coordinate System

Blunder world space is **right-handed, Z-up**:

| Axis | Direction |
|------|-----------|
| +X | Right |
| +Y | Forward |
| +Z | Up |

Constants and glTF import helpers live in
[`coordinate_system.h`](engine/src/runtime/core/math/coordinate_system.h).
`Transform` local axes match world (+Y forward, +Z up). The editor ground grid
uses the **XY** plane (`ForwardGridPlane::xy`).

**glTF** sources are Y-up (Khronos default). Runtime import applies a fixed basis
change `(x, y, z) → (x, z, -y)` to mesh vertices/normals/tangents and
`similarityGltfToEngine()` on scene node matrices. Raw `.gltf` / `.glb` on disk
are unchanged; re-cook meshes after changing this mapping
(`asset_compiler --force` or delete `.blunder/cooked/`).

**`.scene.asset`** positions and `euler_degrees` are authored in engine Z-up
(fixed-axis ZYX: rotations about +X, +Y, +Z).

---

## Render Data Flow

The editor uses an off-screen render target + Slint composition pipeline. The
engine itself never presents to the window: Slint's `SkiaRenderer` owns the
HWND and is in charge of `Present`. Per frame:

```
SDL3 pumpEvents
   └─► WindowSystem ─► layers ─► SlintSystem.processEvent (input forwarded)

RenderSystem::tick(dt, viewport_w, viewport_h)
   ├─ resize OffscreenRenderTarget if Slint reports a new central rect size
   ├─ assemble ForwardFrameState + opaque draw list (N mesh sources)
   └─► ForwardRenderPath::renderFrame
         ├─ RHI beginRenderPass (color + depth clear) on offscreen RT
         ├─ opaque draw list [0..N) (basic.slang, per-slot descriptors)
         ├─ transparent meshes
         ├─ scene overlays (axes, wireframe solids — depth-aware, main color)
         ├─ RHI endRenderPass → SHADER_READ_ONLY
   ├─ OverlaySystem::draw_overlay_lines (OverlayLinePass MRT: grid lines → line_tx)
   ├─ OverlaySystem::draw_overlay_aa (overlay_aa.slang → main color)
   ├─ SsaOPass::apply (composite AO onto main color)
   ├─ OverlaySystem::draw_screen_overlays (ScreenOverlayPass LOAD: navigate gizmo)
   ├─ RHI transitionToCopySource → copyColorToBuffer (staging)
   └─ RHI transitionToShaderRead
   ├─ submit + wait fence (single-frame stall)
   └─ map staging → memcpy → viewport presenter → Slint viewport Image

SlintSystem::update()
   ├─ slint::platform::update_timers_and_animations()
   └─ SkiaRenderer.render()  // GPU composite + Present on HWND
            └─ The central `Image` control samples the SharedPixelBuffer
              created from the engine's readback pixels.
```

Key integration points:

| Concern              | Owner                            |
|----------------------|----------------------------------|
| Window / HWND        | `WindowSystem` (SDL3, no `SDL_WINDOW_VULKAN`) |
| Vulkan device        | `VulkanContext` (headless, no surface/swapchain) |
| 3D scene pass        | `ForwardRenderPath` + `OffscreenRenderTarget` (RHI pass API) |
| Per-frame readback   | `RenderSystem::tick` (submit/fence/map after forward path) |
| UI composite + Present | `SlintSystem` + `SkiaRenderer` |
| 3D viewport size     | Slint `viewport-width/height` ► `RenderSystem` |
| 3D pixels into UI    | `SlintSystem::setViewportImage`  |

**Overlay phases** (`OverlaySystem` — screen HUD must not run in the forward
CLEAR pass; SSAO composite clears and rewrites main color):

| Phase | API | When |
|-------|-----|------|
| Scene | `draw_scene_overlays` | Inside forward render pass |
| Lines | `draw_overlay_lines` | After forward; MRT to `OverlayLineTargets` |
| Line AA | `draw_overlay_aa` | Before SSAO; composites onto main color |
| Screen | `draw_screen_overlays` | After SSAO; LOAD pass (`ScreenOverlayPass`) |

Notes / known limitations:

- Slint is source-built from the **fork** submodule (`blunder/v1.16.1` on
  `BearThreeStones/slint`, based on upstream `v1.16.1`). See
  `engine/3rdparty/slint/BLUNDER_PATCHES.md`. `RENDERER_SKIA_VULKAN` stays enabled
  for the build, but the C++ custom platform uses **D3D12** for window composition
  on Windows (engine Vulkan remains headless + CPU readback for the 3D viewport).
- Rebuild target `slint_cpp` whenever the Slint submodule commit changes.
- Zero-copy GPU texture sharing is still not implemented. The public C++ API
  surface currently only exposes `BorrowedOpenGLTexture` and
  `set_rendering_notifier`, so the data flow above remains structured around
  readback until a separate Vulkan texture import path is available.
- The readback uses a synchronous fence wait per frame. Pingponging across
  `VulkanSync::k_max_frames_in_flight` staging buffers (already provisioned)
  is the next optimisation if this becomes a bottleneck.
- `EditorCamera` still receives input in window coordinates; for delta-based
  motion (drag/orbit) this is fine, but absolute-position interactions should
  later be remapped to the central viewport rect.

---

## Slint fork (`engine/3rdparty/slint`)

Submodule URL: `https://github.com/BearThreeStones/slint.git`, branch **`blunder/v1.16.1`**
(pinned by commit in the parent repo).

| Remote | URL |
|--------|-----|
| `origin` | BearThreeStones/slint (fork, push patches here) |
| `upstream` | slint-ui/slint (read-only, version bumps) |

**First-time setup (maintainer):**

```bash
# On GitHub: fork slint-ui/slint -> BearThreeStones/slint (if not done yet)
cd engine/3rdparty/slint
git push -u origin blunder/v1.16.1
cd ../../..
git submodule update --init engine/3rdparty/slint
```

**Clone Blunder Engine:**

```bash
git clone --recurse-submodules https://github.com/BearThreeStones/Blunder-Engine.git
# or after clone: git submodule update --init --recursive engine/3rdparty/slint
```

**Bump Slint version:** rebase `blunder/vX.Y.Z` onto upstream tag, update `SLINT_GIT_TAG` /
`slint.cmake` branch name check, cherry-pick from previous `blunder/*` branch, rebuild `slint_cpp`.

---

## Compiler Defines (MSVC)

- `NOMINMAX` - Disables Windows min/max macros
- `WIN32_LEAN_AND_MEAN` - Minimal Windows headers
- `_CRT_SECURE_NO_WARNINGS` - Suppress CRT warnings
- `/MP` - Multi-processor build
- `/Z7` - Debug info (Debug config)

---

## Cursor Cloud specific instructions

### Linux Build (Cloud Agent Environment)

The project targets Windows/MSVC but builds on Linux using GCC and Ninja for cloud agent development. Key differences:

**CMake configuration (Linux):**
```bash
# The root file is CMakelists.txt (lowercase 'l'); a CMakeLists.txt symlink is needed
ln -sf CMakeLists_linux.cmake CMakeLists.txt

# Configure
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_COMPILER=gcc \
  -DSDL_WAYLAND=OFF \
  -DCMAKE_CXX_FLAGS="-DEASTL_USER_DEFINED_ALLOCATOR"

# Build
cmake --build build

# Run (requires DISPLAY and library paths)
export DISPLAY=:1
export LD_LIBRARY_PATH="/workspace/.cmake_deps/slang_sdk-src/lib:/workspace/.cmake_deps/slint-sdk-1.16.1/install/lib:$LD_LIBRARY_PATH"
./build/engine/src/editor/engine_editor
```

**Key gotchas:**
- `EASTL_USER_DEFINED_ALLOCATOR` must be defined globally; without it, EASTL's inline allocator implementations conflict with the project's custom `eastl_allocator.cpp`.
- `cmake/slint.cmake` and `cmake/slang.cmake` are Windows-only; the Linux build uses `cmake/slint_linux.cmake` and `cmake/slang_linux.cmake` (included by `CMakeLists_linux.cmake`).
- `engine/3rdparty/cgltf/` is header-only with no upstream CMakeLists.txt; a minimal one must exist for `add_subdirectory()` to work.
- SDL3 on Linux requires `-DSDL_WAYLAND=OFF` (no Wayland compositor in the VM).
- Vulkan validation errors about semaphores are expected with Mesa's software Vulkan driver (lavapipe); they do not affect functionality.
- The engine runs at DISPLAY=:1 which is the VM's pre-configured virtual display.

**Lint:**
```bash
# Format check (no root .clang-format config exists; uses default style)
clang-format --dry-run --Werror engine/src/runtime/**/*.cpp engine/src/runtime/**/*.h

# Static analysis
clang-tidy engine/src/runtime/engine.cpp -- -std=c++20 -Iengine/src -I...
```

**No tests exist** — the only validation is a successful build + run.
