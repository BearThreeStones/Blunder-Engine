# AGENTS.md - Blunder Engine

> AI agent reference for the Blunder Engine codebase (a.k.a. ToyEngine).

## Project Overview

C++20 game engine using CMake, targeting Windows with Visual Studio 2026.

**Tech Stack:** C++20 | CMake 3.21+ | Vulkan | VS 2026

**Dependencies (git submodules):** glm, glfw, spdlog, imgui, EASTL

---

## Build Commands

```bash
# Configure
cmake --preset vs2026-debug

# Build Debug
cmake --build build/vs2026-debug --config Debug

# Build Release
cmake --build build/vs2026-debug --config Release

# Build specific target
cmake --build build/vs2026-debug --config Debug --target engine_editor

# Open in Visual Studio
start build/vs2026-debug/ToyEngine.sln

# Run editor
./build/vs2026-debug/engine/src/editor/Debug/engine_editor.exe
```

---

## Testing

> **Note:** No test infrastructure yet. When adding tests, use CTest and create `engine/src/tests/`.

---

## Directory Structure

```
engine/
├── src/
│   ├── runtime/           # Core library (engine_runtime)
│   │   ├── core/          # Base utilities (logging, macros)
│   │   └── function/      # Systems (input, global context)
│   └── editor/            # Editor executable (engine_editor)
└── 3rdparty/              # Git submodules
```

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

## Render Data Flow

The editor uses an off-screen render target + Slint composition pipeline. The
engine itself never presents to the window: Slint's `SkiaRenderer` owns the
HWND and is in charge of `Present`. Per frame:

```
SDL3 pumpEvents
   └─► WindowSystem ─► layers ─► SlintSystem.processEvent (input forwarded)

RenderSystem::tick(dt, viewport_w, viewport_h, slint_system)
   ├─ resize OffscreenRenderTarget if Slint reports a new central rect size
   ├─ pass 1: scene pipeline (basic.slang) → OffscreenRenderTarget
   │            (image format R8G8B8A8_UNORM, finalLayout SHADER_READ_ONLY)
   ├─ image barrier: SHADER_READ_ONLY → TRANSFER_SRC
   ├─ vkCmdCopyImageToBuffer → host-visible staging buffer (VMA GPU_TO_CPU)
   ├─ image barrier: TRANSFER_SRC → SHADER_READ_ONLY
   ├─ submit + wait fence (single-frame stall)
   └─ map staging → memcpy → SlintSystem.setViewportImage(rgba8 pixels)

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
| 3D scene pass        | `VulkanPipeline` + `OffscreenRenderTarget` |
| Per-frame readback   | `RenderSystem::tick`             |
| UI composite + Present | `SlintSystem` + `SkiaRenderer` |
| 3D viewport size     | Slint `viewport-width/height` ► `RenderSystem` |
| 3D pixels into UI    | `SlintSystem::setViewportImage`  |

Notes / known limitations:

- Slint is now expected to be source-built so `RENDERER_SKIA_VULKAN` can be
  enabled and `RENDERER_SKIA_OPENGL` can be disabled at configure time. The
  runtime still keeps the 3D viewport on the existing CPU readback path; this
  change only switches Slint/Skia's window composition path to Vulkan.
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

## Compiler Defines (MSVC)

- `NOMINMAX` - Disables Windows min/max macros
- `WIN32_LEAN_AND_MEAN` - Minimal Windows headers
- `_CRT_SECURE_NO_WARNINGS` - Suppress CRT warnings
- `/MP` - Multi-processor build
- `/Z7` - Debug info (Debug config)
