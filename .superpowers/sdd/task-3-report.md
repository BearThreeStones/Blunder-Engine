# Task 3 Report: Managed Api + ScriptHost hook

## Status: DONE

## TDD Evidence

### RED (Step 1)

Updated `NativeAbiTests/Program.cs`: `sizeof(BlunderNativeAbi) == 23 * sizeof(nint)` and four message stub pointers.

```powershell
dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug
```

**Result:** exit 1 — compile errors (`BlunderMessageArg` not found; `message_register` etc. missing on `BlunderNativeAbi`).

### GREEN (Steps 2–5)

Implemented:

- `MessageArg.cs` — `MessageArgKind`, `MessageArg`, `BlunderMessageArg` (Explicit 16-byte layout: kind + 7 pad + union)
- `Message.cs` — `MessageId`, `Message.Register` / `Message.Send` (max 4 args)
- `Behaviour.OnMessage` virtual
- `NativeAbi.cs` / `Native.cs` — four `message_*` fields + wrappers
- `HostExports.cs` — `OnMessage` hook; `RegisterLifecycleHooks` sets hook; `ShutdownCleanup` clears hook

```powershell
dotnet build engine/managed/Blunder.Api/Blunder.Api.csproj -c Debug
dotnet build engine/managed/Blunder.ScriptHost/Blunder.ScriptHost.csproj -c Debug
dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug
```

**Result:** all exit 0. NativeAbiTests output: `Blunder.Api.NativeAbiTests: OK`.

## Commit

```
feat: add Message façade and Behaviour.OnMessage hook
```

## Test Summary

| Test | Result |
|------|--------|
| `Blunder.Api` build | PASS |
| `Blunder.ScriptHost` build | PASS |
| `Blunder.Api.NativeAbiTests` | PASS (23-pointer layout, register/send stubs) |

## Concerns

- No e2e message dispatch test yet (Task 4).
- `BlunderMessageArg` layout verified by Explicit `Size = 16` matching C header; no cross-language struct-size assertion in managed tests.

## Files Changed

- `engine/managed/Blunder.Api/MessageArg.cs` (new)
- `engine/managed/Blunder.Api/Message.cs` (new)
- `engine/managed/Blunder.Api/Behaviour.cs`
- `engine/managed/Blunder.Api/NativeAbi.cs`
- `engine/managed/Blunder.Api/Native.cs`
- `engine/managed/Blunder.ScriptHost/HostExports.cs`
- `engine/managed/Blunder.Api.NativeAbiTests/Program.cs`
