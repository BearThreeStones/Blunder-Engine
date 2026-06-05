# Cursor Cloud (Linux agent environment)

The project targets Windows/MSVC but builds on Linux using GCC and Ninja for cloud agent development.

## Linux build

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

## Key gotchas

- `EASTL_USER_DEFINED_ALLOCATOR` must be defined globally; without it, EASTL's inline allocator implementations conflict with the project's custom `eastl_allocator.cpp`.
- `cmake/slint.cmake` and `cmake/slang.cmake` are Windows-only; the Linux build uses `cmake/slint_linux.cmake` and `cmake/slang_linux.cmake` (included by `CMakeLists_linux.cmake`).
- `engine/3rdparty/cgltf/` is header-only with no upstream CMakeLists.txt; a minimal one must exist for `add_subdirectory()` to work.
- SDL3 on Linux requires `-DSDL_WAYLAND=OFF` (no Wayland compositor in the VM).
- Vulkan validation errors about semaphores are expected with Mesa's software Vulkan driver (lavapipe); they do not affect functionality.
- The engine runs at DISPLAY=:1 which is the VM's pre-configured virtual display.

## Lint

```bash
# Format check (no root .clang-format config exists; uses default style)
clang-format --dry-run --Werror engine/src/runtime/**/*.cpp engine/src/runtime/**/*.h

# Static analysis
clang-tidy engine/src/runtime/engine.cpp -- -std=c++20 -Iengine/src -I...
```

**No tests exist** — the only validation is a successful build + run.

## See also

- [build.md](build.md) — Windows presets
- [slint-fork.md](slint-fork.md)
- [code-style.md](code-style.md)
- [testing.md](testing.md)
