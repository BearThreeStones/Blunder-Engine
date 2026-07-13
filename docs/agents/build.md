# Build Commands

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

# Default scene is `assets/Scenes/pick_test.scene.asset`; override:
# set BLUNDER_STARTUP_SCENE=assets/Scenes/root.scene.asset
```

## CPU requirements (Windows)

MSVC engine targets (e.g. `engine_runtime`) are built with `/arch:AVX2` so GLM can use AVX2 matrix/vector intrinsics. The editor requires an **x64 CPU with AVX2** (roughly Intel Haswell 2013+ or AMD Excavator 2015+). Pre-AVX2 hardware will not run Windows editor binaries from this configuration.

Linux GCC builds use `GLM_FORCE_INTRINSICS` without the MSVC `/arch:AVX2` flag; SIMD level follows the compiler's default x86_64 ISA.

## See also

- [CMakePresets.json](../../CMakePresets.json)
- [cursor-cloud.md](cursor-cloud.md) — Linux / cloud agent build
- [slint-fork.md](slint-fork.md) — rebuild `slint_cpp` after submodule changes
- [testing.md](testing.md)
