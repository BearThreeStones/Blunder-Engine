# Testing

First-party tests live under `engine/src/tests/` and are wired with CTest via `engine/src/tests/CMakeLists.txt`.

## Run

```powershell
cmake --build build/vs2026-debug --config Debug --target classdb_test
.\build\vs2026-debug\engine\src\tests\Debug\classdb_test.exe

# Or, from the build tree:
ctest --test-dir build/vs2026-debug -C Debug -R "classdb|variant|object_db|ptrcall|engine_c_abi|dotnet_host|editor_history|scene_soft_delete|editor_commands|scene_serializer|scene_behaviour|play_" --output-on-failure
```

Tests that only pull ClassDB/Object (not SceneInstance/Vulkan) stay small and do not need `slang.dll`. Prefer `IEntityStore` fakes over linking `SceneInstance` in unit tests.

`scene_soft_delete_test` / `editor_commands_test` link `SceneInstance` and may need `slang.dll` + `slint_cpp.dll` on `PATH` (e.g. VulkanSDK Bin + `.cmake_deps/slint-build`).

## .NET scripting host

Prerequisite: **.NET 10 SDK + runtime** installed (set `DOTNET_ROOT` if CMake cannot find the host pack / `nethost`).

| Target | Focus |
|--------|--------|
| `dotnet_host_test` | Approach A: CoreCLR ScriptHost + fixture game + managed `ProbeBehaviour.Tick` via LoadLibrary’d SHARED `blunder_engine_c` (registered module ABI) |
| `editor_dotnet_host_test` | Editor-style: process ObjectDB + `blunder_native_abi_fill_from_process` + managed Tick (links `blunder_engine_c_static`; no SHARED Object traffic) |
| `scene_serializer_test` | Scene JSON including entity `behaviours` (type / BehaviourId / optional property bag); legacy scenes without the key remain valid |
| `scene_behaviour_instantiate_test` | Load with behaviours: bind Object to EntityId, restore slots/ids, null peers when DotNetHost is not running |
| `scene_behaviour_mount_test` | Load + process ABI + DotNetHost + fixture type → AttachBehaviour / managed Tick (editor-style, like `editor_dotnet_host_test`) |
| `scene_behaviour_export_test` | Export Scene from live instance writes Behaviour list from bound Objects; tombstoned entities stay omitted |

**Behaviour serialization:** Scene entities may declare an ordered `behaviours` array. Instantiation creates/binds an Object only when the list is non-empty, restores BehaviourIds, and advances next-id. Peers mount only when `DotNetHost` is running; offline load must succeed. Export reads type + id from bound Objects (property bag on export may be empty in this MVP). Requires the single-ObjectDB / NativeAbi contract from `unify-script-objectdb` (already on main).

**Single-ObjectDB contract:** Editor and managed ScriptHost/Api share one ObjectDB via a native-registered C-ABI function-pointer table (`RegisterNativeAbi`). The editor fills from process-linked symbols (`blunder_native_abi_fill_from_process`); Approach A tests fill from SHARED module exports (`blunder_native_abi_fill_from_module`). Do not link `blunder_engine_c_static` and LoadLibrary SHARED `blunder_engine_c` for Object traffic in the same process.

**Approach A note (`dotnet_host_test`):** Object create and lifecycle invoke go only through the ScriptHost-staged SHARED DLL (`LoadLibrary` of `bin/<Config>/blunder_engine_c.dll`). The test does not link the C-ABI import library. Do not call `ObjectDB` / `LifecycleDispatch` from that test exe against IDs meant for the SHARED image.

**Host start policy (ADR 0014 / play-mode-ui):** Product **Play Mode** runs `DotNetHost` inside the **Player** process (`engine_player`), not the editor. Edit Mode does **not** auto-start a host for authorship or for the Play toolbar.

| Mode | When DotNetHost starts |
|------|------------------------|
| Player (`EngineHostMode::Player`) | Always at `startSystems` (non-fatal if ScriptHost / game DLL missing) |
| Editor | Only if `BLUNDER_DOTNET_SCRIPTS=1` — **debug / test opt-in**, not product Play |

| Env | Effect |
|-----|--------|
| `BLUNDER_DOTNET_SCRIPTS=1` | Editor-only: start ScriptHost from `Blunder.ScriptHost.dll` beside the editor; if `.blunder/scripts_bin/*.dll` exists, also `loadGameAssembly` (non-fatal on failure). Do **not** set this while using editor Play (would start a second host alongside `engine_player`). |
| unset (default) | Editor leaves `m_dotnet_host` null; Play still works via Player |

`RuntimeGlobalContext` owns `DotNetHost`. Frame loop calls `ObjectDB::forEach` → `LifecycleDispatch::invokeReady` / `invokeTick` when the host is running (Player, or editor with the debug env).

### Play session unit tests

| Target | Focus |
|--------|--------|
| `play_pause_tick_gate_test` | Pause skips Behaviour Tick; Resume clears gate |
| `play_ipc_test` | localhost TCP pause / resume / stop + `ready` |
| `play_session_controller_test` | Stopped / Starting / Playing / Paused; spawn lifecycle |
| `play_preflight_test` | Dirty-scene decisions + Scripts dirty build gate |

```powershell
cmake --build build/vs2026-debug --config Debug --target editor_dotnet_host_test
.\build\vs2026-debug\engine\src\tests\Debug\editor_dotnet_host_test.exe

cmake --build build/vs2026-debug --config Debug --target dotnet_host_test
.\build\vs2026-debug\engine\src\tests\Debug\dotnet_host_test.exe

# Play session / IPC / preflight (no CoreCLR required for most):
ctest --test-dir build/vs2026-debug -C Debug -R "play_(pause_tick_gate|ipc|session_controller|preflight)" --output-on-failure

# Debug-only editor host smoke (not product Play):
$env:BLUNDER_DOTNET_SCRIPTS = "1"
.\build\vs2026-debug\bin\Debug\engine_editor.exe --project-root "E:/Blunder Projects/Test"
# Confirm log: [DotNetHost] ScriptHost running (+ loaded game assembly when scripts_bin present)
Remove-Item Env:BLUNDER_DOTNET_SCRIPTS
```

### Manual Player CLI

Stage `engine_player` beside ScriptHost (CMake POST_BUILD). From a Project with a saved scene asset:

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_player
.\build\vs2026-debug\engine\src\player\Debug\engine_player.exe `
  --project-root "E:/Blunder Projects/Test" `
  --scene "assets/Scenes/pick_test.scene.asset"
# Optional IPC (editor PlaySessionController passes this):
#   --play-ipc 127.0.0.1:<port>
# Smoke exit after N frames: $env:BLUNDER_PLAYER_MAX_FRAMES = "30"
```

Expect log: `[DotNetHost] ScriptHost running` (and game assembly load when `.blunder/scripts_bin` has a project DLL). Closing the window exits the process.

## Manual checklist (Play Mode UI)

Use a Project with `Scripts/` scaffold; leave `BLUNDER_DOTNET_SCRIPTS` **unset**. Build `engine_editor` + `engine_player`.

1. **Play → Pause → Resume → Stop** — toolbar Play spawns Player; Pause enabled after IPC `ready`; Pause skips Tick (orbit/render ok); Resume clears; Stop ends session → Stopped.
2. **Close Player window** — process exits; editor returns to Stopped (no hung Starting/Playing).
3. **Dirty scene prompt** — dirty active scene → Play shows Save and Play / Play Last Saved / Cancel; Cancel stays Stopped; Save and Play / Last Saved proceeds to spawn.
4. **Scripts dirty build** — touch a `.cs` under `Scripts/` → Play runs `dotnet build` into `.blunder/scripts_bin`; build fail → stay Stopped with error; success → spawn Player with updated DLL.
5. **Debug host (optional)** — with `BLUNDER_DOTNET_SCRIPTS=1`, editor starts in-process ScriptHost for `editor_dotnet_host_test`-style smoke; Approach A remains `dotnet_host_test`. Not required for product Play.

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
