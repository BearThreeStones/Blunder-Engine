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
| `dotnet_host_test` | Approach A: CoreCLR ScriptHost + fixture game + managed `ProbeBehaviour.Tick` via LoadLibrary’d SHARED `blunder_engine_c` (registered module ABI) |
| `editor_dotnet_host_test` | Editor-style: process ObjectDB + `blunder_native_abi_fill_from_process` + managed Tick (links `blunder_engine_c_static`; no SHARED Object traffic) |

**Single-ObjectDB contract:** Editor and managed ScriptHost/Api share one ObjectDB via a native-registered C-ABI function-pointer table (`RegisterNativeAbi`). The editor fills from process-linked symbols (`blunder_native_abi_fill_from_process`); Approach A tests fill from SHARED module exports (`blunder_native_abi_fill_from_module`). Do not link `blunder_engine_c_static` and LoadLibrary SHARED `blunder_engine_c` for Object traffic in the same process.

**Approach A note (`dotnet_host_test`):** Object create and lifecycle invoke go only through the ScriptHost-staged SHARED DLL (`LoadLibrary` of `bin/<Config>/blunder_engine_c.dll`). The test does not link the C-ABI import library. Do not call `ObjectDB` / `LifecycleDispatch` from that test exe against IDs meant for the SHARED image.

**Editor / runtime:** `RuntimeGlobalContext` owns `DotNetHost`. Frame loop calls `ObjectDB::forEach` → `LifecycleDispatch::invokeReady` / `invokeTick` when the host is running. Play UI is not wired yet, so **host start** is gated:

| Env | Effect |
|-----|--------|
| `BLUNDER_DOTNET_SCRIPTS=1` | Start ScriptHost from `Blunder.ScriptHost.dll` beside the editor; if `.blunder/scripts_bin/*.dll` exists, also `loadGameAssembly` (non-fatal on failure) |

```powershell
cmake --build build/vs2026-debug --config Debug --target editor_dotnet_host_test
.\build\vs2026-debug\tests\Debug\editor_dotnet_host_test.exe
# or: .\build\vs2026-debug\engine\src\tests\Debug\editor_dotnet_host_test.exe

cmake --build build/vs2026-debug --config Debug --target dotnet_host_test
.\build\vs2026-debug\tests\Debug\dotnet_host_test.exe

# Editor smoke (host + Scripts load when scripts_bin present):
$env:BLUNDER_DOTNET_SCRIPTS = "1"
.\build\vs2026-debug\bin\Debug\engine_editor.exe --project-root "E:/Blunder Projects/Test"
```

## Manual checklist (editor Scripts / Play)

1. Create/open a Project with `Scripts/` scaffold
2. `dotnet build` via `ScriptsBuilder` (or `dotnet build` into `.blunder/scripts_bin`)
3. Start editor with `--project-root` and `BLUNDER_DOTNET_SCRIPTS=1`
4. Confirm log: `[DotNetHost] ScriptHost running` and, when a game DLL exists, `[DotNetHost] loaded game assembly ...`
5. Managed Tick on process Objects is proven by `editor_dotnet_host_test`; Approach A remains `dotnet_host_test`

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
