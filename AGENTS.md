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
