## Why

The Reflection kernel (ClassDB, C-ABI, Object, lifecycle skeleton) is closed, but Projects still cannot run C# gameplay. ADR 0011 locks C# Behaviours as the sole first-class scripting model; without an in-process .NET host, DogWalk and every Project script remain blocked behind a single unused Script Peer slot.

## What Changes

- Embed **CoreCLR** via `nethost` / `hostfxr` and load a managed **Blunder.ScriptHost**
- Replace Object’s single Script Peer with an ordered **Behaviour** list (`BehaviourId`, type name, peer)
- Dispatch **Ready/Tick** once per Behaviour peer in list order
- Ship hand-authored **Blunder.Api** (`net10.0`) over the C-ABI; Create scaffolds Project **`Scripts/`** + `.csproj`
- Editor/dev path invokes **`dotnet build`** on `Scripts/` and loads one game assembly
- Extend **C-ABI** for Behaviour attach/peer and Vec3 property get/set (**BREAKING** ABI version bump for script hosts; native editor callers of v1 bool-only surface remain source-compatible if they ignore new symbols)
- **Out of scope:** ALC hot reload, Behaviour scene serialization, Inspector Behaviour UX, NuGet publish, file-watcher auto-build, DogWalk content itself

## Capabilities

### New Capabilities
- `dotnet-script-host`: In-process CoreCLR host, ScriptHost entry points, game assembly load, managed lifecycle registration
- `csharp-behaviour`: Behaviour list on Object, BehaviourId, Ready/Tick per peer, Blunder.Api Behaviour/ObjectHandle surface
- `project-scripts`: Scripts root layout, Create scaffold, `dotnet build` into `.blunder/scripts_bin`, Api HintPath beside editor

### Modified Capabilities
- `engine-c-abi`: Add Behaviour and Vec3 entry points; bump ABI version; lifecycle Ready register already present — document Behaviour peer semantics
- `classdb`: Lifecycle requirement text — dispatch per Script Peer on the Behaviour list (kernel single-slot → multi-peer)
- `object-identity`: Object hosts zero or more Behaviours / Script Peers (amends prior ≤1 Script Peer wording)
- `project-file`: Create Project also scaffolds `Scripts/` + minimal game `.csproj` when this change ships

## Impact

- Native: `Object` Behaviour list, `LifecycleDispatch`, `ObjectDB::forEach`, `DotNetHost`, `ScriptsBuilder`, shared `blunder_engine_c` (or equivalent) for `DllImport`
- Managed: `engine/managed/Blunder.Api`, `engine/managed/Blunder.ScriptHost` (`net10.0`)
- Project Manager Create path + `project_create` tests
- Engine frame optionally drives Behaviour Tick when host is running
- Build/dev dependency: **.NET 10 SDK + runtime** on machines that build or run script tests
- ADRs: `0011-csharp-behaviour-scripting.md` (already accepted); update `0005` handoff already done
- Implementation plan: `docs/superpowers/plans/2026-07-20-dotnet-host-mvp.md`
- Glossary: `CONTEXT.md` (.NET host MVP, Scripts root, Behaviour)
