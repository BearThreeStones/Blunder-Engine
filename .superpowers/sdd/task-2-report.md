# Task 2 Report: Managed registration path for unify-script-objectdb

## Status

**DONE_WITH_CONCERNS**

## Summary

Wired ScriptHost `RegisterNativeAbi` and Blunder.Api `Native` through the `BlunderNativeAbi` function-pointer table. Calls before registration throw `InvalidOperationException` (no `DllImport` fallback). Shared ScriptHost ALC for Blunder.Api preserved so game assemblies see the same registered table. OpenSpec 2.1–2.3 marked `[x]`.

## Commits

- `e0d7127bc122b269c2de46b9c4594345dafbac42` — `feat(script): register native C-ABI table into Blunder.Api`

## Files changed

| Path | Action |
|------|--------|
| `engine/managed/Blunder.Api/NativeAbi.cs` | Created — managed POD mirror of C `BlunderNativeAbi` |
| `engine/managed/Blunder.Api/Native.cs` | Rewritten — register + call through pointers |
| `engine/managed/Blunder.Api/Blunder.Api.csproj` | Modified — `AllowUnsafeBlocks`; InternalsVisibleTo tests |
| `engine/managed/Blunder.ScriptHost/HostExports.cs` | Modified — `RegisterNativeAbi` export; ALC comment |
| `engine/managed/Blunder.Api.NativeAbiTests/*` | Created — TDD harness (stub ABI) |
| `openspec/changes/unify-script-objectdb/tasks.md` | Modified — 2.1–2.3 `[x]` |

## API delivered

- `BlunderNativeAbi` (C# sequential struct, 17 cdecl function pointers)
- `Native.Register(in BlunderNativeAbi)` — rejects incomplete tables
- `Native.ClearRegistrationForTests()` — test seam
- `HostExports.RegisterNativeAbi(BlunderNativeAbi*)` — `UnmanagedCallersOnly` / cdecl; returns Ok/Error
- All former `DllImport` Native methods now invoke registered pointers (UTF-8 string marshal)

## TDD evidence

### RED

Added `Blunder.Api.NativeAbiTests` before production types existed.

```text
dotnet build engine/managed/Blunder.Api.NativeAbiTests -c Debug
```

**Result:** exit 1 — `CS0246` (`BlunderNativeAbi` missing), `CS0122` (`Native` inaccessible / no Register). Expected red.

### GREEN

Implemented `NativeAbi.cs`, pointer-table `Native.cs`, `RegisterNativeAbi`, InternalsVisibleTo.

```text
dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug  → exit 0
dotnet build engine/managed/Blunder.ScriptHost -c Debug                 → exit 0
```

Checks: throw before register; stub table succeeds; incomplete register rejected; layout size 136 (17×8).

## Self-review

- **Correctness:** Hard-fail unregistered; no DllImport path remains in Api.
- **Scope:** No DotNetHost wiring / gate removal (Task 3–4).
- **ALC (2.3):** Existing `GameAssemblyLoadContext` still returns ScriptHost’s Blunder.Api; comment notes registration visibility.

## Concerns

1. **`dotnet_host_test` will fail until Task 3** — ScriptHost/Api no longer DllImport SHARED `blunder_engine_c`; DotNetHost must call `RegisterNativeAbi` before hooks / game load.
2. **Managed test not in CTest** — run via `dotnet run --project engine/managed/Blunder.Api.NativeAbiTests`.
3. **Dirty working tree** — unrelated WIP left uncommitted (Assets/PM/etc.); commit scoped to managed + OpenSpec tasks only.
