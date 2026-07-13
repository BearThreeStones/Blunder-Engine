# Reflection export (Clang)

Generates ClassDB registration (`.gen.cpp`) and `extension_api.json` (API Blueprint) from headers annotated with `BLUNDER_CLASS` / `BLUNDER_PROPERTY` / `BLUNDER_FUNCTION`.

## Requirements

- Python 3.10+
- LLVM/Clang with libclang (this repo's Slint build already expects `C:/Program Files/LLVM`)
- `pip install libclang` (or set `LIBCLANG_PATH` to `clang.dll` / `libclang.dll`)

## MSVC compile flags

The engine compiles with MSVC. The exporter parses with Clang and needs a compilation database or explicit `-I` / `-D` flags.

### Option A — `compile_commands.json` (recommended)

```powershell
# From a configured build tree that emits compile_commands (Ninja) or:
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON --preset vs2026-debug
# If the VS generator does not emit it, use a Ninja preset or copy from a clang-cl configure.
```

Then:

```powershell
python tools/reflection_export/export_reflection.py `
  --compile-commands build/vs2026-debug/compile_commands.json `
  --header engine/src/runtime/core/object/object.h `
  --out-dir engine/src/runtime/core/reflection/generated
```

### Option B — explicit includes

```powershell
python tools/reflection_export/export_reflection.py `
  --header engine/src/runtime/core/object/object.h `
  -- -Iengine/src -Iengine/3rdparty/EASTL/include -Iengine/3rdparty/glm `
  -DBLUNDER_REFLECTION_EXPORT=1
```

## Regenerate pilots

Checked-in outputs live under `engine/src/runtime/core/reflection/generated/`. After changing annotated headers, regenerate and commit both `.gen.cpp` and `extension_api.json`.

CMake does **not** run the exporter on every build (Clang tooling is optional on developer machines). Regeneration is a documented manual/CI step.
