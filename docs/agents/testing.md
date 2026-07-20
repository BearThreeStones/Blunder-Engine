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

**Dual-ObjectDB note (tests):** native `engine_runtime` and SHARED `blunder_engine_c` each have their own ObjectDB/ClassDB statics. `dotnet_host_test` uses **Approach A** — Object create and lifecycle invoke go only through the ScriptHost-staged SHARED DLL (`LoadLibrary` of `bin/<Config>/blunder_engine_c.dll`, then `blunder_object_create` / `blunder_lifecycle_invoke_*`). The test does not link the C-ABI import library (avoids a second DLL copy next to the exe). Do not call `ObjectDB` / `LifecycleDispatch` from the test exe against IDs meant for managed ScriptHost.

**Editor / runtime (Task 8):** `RuntimeGlobalContext` owns `DotNetHost`. Frame loop calls `ObjectDB::forEach` → `LifecycleDispatch::invokeReady` / `invokeTick` when the host is running. Play UI is not wired yet, so host start is gated:

| Env | Effect |
|-----|--------|
| `BLUNDER_DOTNET_SCRIPTS=1` | Lazy-start ScriptHost from `Blunder.ScriptHost.dll` beside the editor (non-fatal on failure) |
| `BLUNDER_DOTNET_LOAD_SCRIPTS=1` | Also call `loadGameAssembly` on `.blunder/scripts_bin/*.dll` (**experimental**) |

**Dual-ObjectDB blocker (editor):** ScriptHost/Api still `DllImport("blunder_engine_c")`, which loads a **second** ObjectDB image. Editor scene Objects live in the statically linked `engine_runtime` ObjectDB. Until C-ABI is unified (native-registered function pointers into the editor process, or DllImport resolving to the same image), keep `BLUNDER_DOTNET_LOAD_SCRIPTS` off for normal editor use. Tick wiring still runs safely when only the host is up (no managed peers on editor Objects yet). Automated Tick proof remains `dotnet_host_test`.

```powershell
cmake --build build/vs2026-debug --config Debug --target dotnet_host_test
.\build\vs2026-debug\tests\Debug\dotnet_host_test.exe
# or: .\build\vs2026-debug\engine\src\tests\Debug\dotnet_host_test.exe

# Editor smoke (host start, no Scripts load — dual-ObjectDB gated):
$env:BLUNDER_DOTNET_SCRIPTS = "1"
.\build\vs2026-debug\bin\Debug\engine_editor.exe --project-root "E:/Blunder Projects/Test"
```

## Manual checklist (editor Scripts / Play)

1. Create/open a Project with `Scripts/` scaffold
2. `dotnet build` via `ScriptsBuilder` (or `dotnet build` into `.blunder/scripts_bin`)
3. Start editor with `--project-root` and `BLUNDER_DOTNET_SCRIPTS=1`
4. Confirm log: `[DotNetHost] ScriptHost running` (or a non-fatal start warning)
5. Full managed Tick on editor Objects requires dual-ObjectDB unification (see above); until then use `dotnet_host_test`

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
