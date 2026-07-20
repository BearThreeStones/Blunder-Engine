# Testing

First-party tests live under `engine/src/tests/` and are wired with CTest via `engine/src/tests/CMakeLists.txt`.

## Run

```powershell
cmake --build build/vs2026-debug --config Debug --target classdb_test
.\build\vs2026-debug\engine\src\tests\Debug\classdb_test.exe

# Or, from the build tree:
ctest --test-dir build/vs2026-debug -C Debug -R "classdb|variant|object_db|ptrcall|engine_c_abi|dotnet_host|editor_history|scene_soft_delete|editor_commands" --output-on-failure
```

Tests that only pull ClassDB/Object (not SceneInstance/Vulkan) stay small and do not need `slang.dll`. Prefer `IEntityStore` fakes over linking `SceneInstance` in unit tests.

`scene_soft_delete_test` / `editor_commands_test` link `SceneInstance` and may need `slang.dll` + `slint_cpp.dll` on `PATH` (e.g. VulkanSDK Bin + `.cmake_deps/slint-build`).

## .NET scripting host

Prerequisite: **.NET 10 SDK + runtime** installed (set `DOTNET_ROOT` if CMake cannot find the host pack / `nethost`).

| Target | Focus |
|--------|--------|
| `dotnet_host_test` | CoreCLR ScriptHost start, fixture game load, managed `ProbeBehaviour.Tick` via SHARED `blunder_engine_c` |

**Dual-ObjectDB note:** native `engine_runtime` and SHARED `blunder_engine_c` each have their own ObjectDB/ClassDB statics. `dotnet_host_test` uses **Approach A** — Object create and lifecycle invoke go only through the ScriptHost-staged SHARED DLL (`LoadLibrary` of `bin/<Config>/blunder_engine_c.dll`, then `blunder_object_create` / `blunder_lifecycle_invoke_*`). The test does not link the C-ABI import library (avoids a second DLL copy next to the exe). Do not call `ObjectDB` / `LifecycleDispatch` from the test exe against IDs meant for managed ScriptHost.

```powershell
cmake --build build/vs2026-debug --config Debug --target dotnet_host_test
.\build\vs2026-debug\tests\Debug\dotnet_host_test.exe
# or: .\build\vs2026-debug\engine\src\tests\Debug\dotnet_host_test.exe
```

## Reflection kernel coverage

| Target | Focus |
|--------|--------|
| `classdb_test` | ClassDB register/lookup |
| `variant_test` | Variant kinds |
| `object_db_test` | ObjectId, Scene Tree, Script Peer, lazy Entity via fake store |
| `ptrcall_lifecycle_test` | PtrCall, Variant properties, lifecycle hooks |
| `engine_c_abi_test` | C-ABI smoke (no .NET), including Ready hook registration |

Kernel handoff: Object Behaviour lists and the .NET host MVP are covered by ADR 0011; Reflection kernel closure remains ADR 0005.

## Editor History coverage

| Target | Focus |
|--------|--------|
| `editor_history_test` | DocumentHistory stack, dirty baseline, selection restore |
| `scene_soft_delete_test` | Soft-delete EntityId stability; export omits tombstones |
| `editor_commands_test` | Transform / spawn / soft-delete command undo/redo |

## See also

- [cmake.md](cmake.md)
- [build.md](build.md)
- [golden-principles.md](../golden-principles.md)
- [common-tasks.md](common-tasks.md)
- `tools/reflection_export/README.md`
