## Context

Reflection kernel is done: ClassDB, Clang pilot export, API Blueprint, Object/ObjectId, PtrCall/Variant, lifecycle table, C-ABI v1 without .NET. ADR 0011 and `CONTEXT.md` lock C# as the sole gameplay scripting language with Unity-style multi-Behaviour Objects, CoreCLR hosting, `Scripts/` layout, and `net10.0`. Implementation outline lives in `docs/superpowers/plans/2026-07-20-dotnet-host-mvp.md`. Stakeholders: engine runtime, Project Manager Create, future DogWalk C# vertical slice.

## Goals / Non-Goals

**Goals:**
- In-process CoreCLR host (`nethost` / `hostfxr`) loading Blunder.ScriptHost then one Project game assembly
- Ordered Behaviour list on Object with stable BehaviourId; Ready/Tick per peer in list order
- Hand-authored Blunder.Api (`net10.0`) over C-ABI; Create scaffolds `Scripts/` + thin `.csproj`
- Dev `dotnet build` of Scripts into `.blunder/scripts_bin`; engine can drive Tick when host is running
- Native + managed smoke tests proving managed Tick without shipping the full editor Play UX

**Non-Goals:**
- ALC hot reload
- Behaviour scene serialization / Inspector Behaviour UX
- NuGet distribution of Blunder.Api (HintPath beside editor is enough)
- File-watcher auto-build
- Generating full Blunder.Api from API Blueprint (hand-authored MVP; generator later)
- DogWalk content/assets port

## Decisions

1. **Multi-Behaviour storage on Object (not ECS Components)**  
   Match ADR 0011. Replace `void* m_script_peer` with an ordered slot list `{BehaviourId, type_name, script_peer}`.  
   *Rejected:* scripts on ECS components; Godot one-script-per-Object as product rule.

2. **Lifecycle: native iterates peers, one managed hook unboxes GCHandle**  
   C++ `LifecycleDispatch` walks the Behaviour list and calls the registered Tick/Ready hook per non-null peer. ScriptHost registers a single unmanaged callback that resolves `GCHandle` → `Behaviour.Tick`/`Ready` (virtual).  
   *Rejected:* managed-only world walk as the sole tick path; per-C#-subclass native hook tables in MVP.

3. **CoreCLR via nethost/hostfxr loading Blunder.ScriptHost**  
   Native `DotNetHost` finds hostfxr, initializes from ScriptHost `.runtimeconfig.json`, resolves `UnmanagedCallersOnly` exports (load game assembly, attach Behaviour, register hooks).  
   *Rejected:* Mono embed; out-of-process `dotnet` IPC as shipping model.

4. **Shared `blunder_engine_c` DLL for DllImport**  
   Static `engine_runtime` cannot be P/Invoked. Export C-ABI from a SHARED library linked against `engine_runtime` (or equivalent export strategy) so Blunder.Api can `DllImport`.  
   *Rejected:* copying generated C# stubs into every Project; scraping C++ headers.

5. **C-ABI version bump to 2**  
   Add Behaviour attach/peer APIs and Vec3 property get/set while keeping existing Object/bool/PtrCall symbols.  
   *Rejected:* Variant-only interop for all managed calls.

6. **Project `Scripts/` + Create scaffold; build to `.blunder/scripts_bin`**  
   Not part of Assets/Resources content pipeline. Create writes `Scripts/<Name>.csproj` referencing Blunder.Api via HintPath to the editor output directory.  
   *Rejected:* storing `.cs` under `Assets/`; requiring authors to build only outside the editor as the primary loop.

7. **TFM `net10.0`**  
   Current .NET LTS for Api, ScriptHost, and Project templates.

## Risks / Trade-offs

- **[.NET 10 missing on a machine] →** Document SDK/runtime prerequisite; `dotnet_host_test` fails clearly; editor must not crash if host start fails (log + continue).
- **[DllImport / DLL search path] →** Copy `blunder_engine_c.dll` + managed DLLs beside `bin/<Config>/` and test exe; pin `NativeLibrary` load path in ScriptHost if needed.
- **[Ready-once semantics] →** Track `ready_invoked` per Behaviour slot so frame Tick does not re-Ready every frame.
- **[Host init cost] →** Start host lazily when Scripts output exists or when explicitly enabled; do not block editor cold start forever on missing runtime.
- **[Hand-authored Api drifts from Blueprint] →** Accept for MVP; follow-up generates Api from `extension_api.json`.

## Migration Plan

1. Land native Behaviour list + lifecycle + C-ABI v2 (engine still runs without .NET).
2. Land managed Api/ScriptHost + host smoke test.
3. Extend Project Create scaffold; existing Projects gain `Scripts/` on demand or manual add (Create path only required for new Projects).
4. Wire optional engine-frame Tick when host running.
5. Rollback: feature-flag / omit DotNetHost start; C-ABI v2 remains backward-compatible for v1 callers that ignore new symbols.

## Open Questions

- Exact Play-mode UX gate (`BLUNDER_DOTNET_SCRIPTS` env vs automatic load when `scripts_bin` exists) — **resolved:** require `BLUNDER_DOTNET_SCRIPTS=1` to start ScriptHost (Play UI missing). Game assembly load is separately gated by `BLUNDER_DOTNET_LOAD_SCRIPTS=1` until dual-ObjectDB unification (document in `docs/agents/testing.md`).
- Whether `blunder_engine_c` is a separate SHARED target or `engine_editor` exports — prefer dedicated SHARED for tests and managed DllImport (**shipped**). Editor still needs a follow-up so DllImport resolves to the **same** ObjectDB as `engine_runtime` (function pointers or main-module exports).
- Editor-frame `LifecycleDispatch` is wired when ScriptHost is running, but **managed Tick on editor scene Objects is not claimed** until DllImport uses that same ObjectDB. Proven path today: `dotnet_host_test` against SHARED `blunder_engine_c`.
