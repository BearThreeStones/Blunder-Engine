# Task 4 Report: DotNet Message e2e smoke

**Branch:** `feat/object-message-communication`  
**Status:** GREEN  
**Date:** 2026-07-22

## Scope

End-to-end proof that native C-ABI `blunder_message_send` (process ObjectDB path) delivers to a managed `Behaviour.OnMessage` on a **different** Object via ScriptHost's message hook.

## TDD evidence

### RED (pre-seam / pre-fixture)

Prior WIP had the test source and fixture Behaviour but no HostExports read seams or DotNetHost resolution. Expected failure modes:

1. **Missing fixture** — `attachBehaviour` fails for `DotnetHostGame.MessageProbeBehaviour`.
2. **Missing `GetMessageProbeCount` export** — `resolveProbeTickCount` fails resolving `HostExports.GetMessageProbeCount`, or `getMessageProbeCount()` returns `-1` → `FAIL OnMessage fired once`.

Simulated RED (logic): with probe seams unwired, assertion `count == 1` fails with `getMessageProbeCount=-1 (expected 1)`.

### GREEN

```text
> cmake --build build/vs2026-debug --config Debug --target object_message_dotnet_test
  object_message_dotnet_test.vcxproj -> ...\object_message_dotnet_test.exe

> .\build\vs2026-debug\engine\src\tests\Debug\object_message_dotnet_test.exe
object_message_dotnet_test OK
```

## What shipped

| Area | Change |
|------|--------|
| Fixture | `MessageProbeBehaviour.cs` — static `MessageCount` / `LastId`, `OnMessage` increments |
| Test | `object_message_dotnet_test.cpp` — process ABI, DotNetHost, attach probe on **target**, register `"Ping"`, C-ABI send, assert count/id; second **other** Object gets send without incrementing probe |
| ScriptHost | `GetMessageProbeCount`, `GetMessageProbeLastId`, `ReadGameStaticUInt` |
| DotNetHost | Resolve + accessors for message probe exports (mirrors tick probe pattern) |
| CMake | `object_message_dotnet_test` target (PRE_BUILD DotnetHostGame, nethost copy, script_host deps) |
| OpenSpec | Tasks 4.1, 4.2 marked complete |

## Test flow

1. `ObjectDB::create()` → `target` + `other`
2. `blunder_native_abi_fill_from_process` → `message_register` / `message_send`
3. `DotNetHost::start` → `RegisterLifecycleHooks` sets managed message hook
4. Load `DotnetHostGame.dll`, attach `MessageProbeBehaviour` on `target`
5. `blunder_message_register("Ping")` + `blunder_message_send(target, ping_id, …)`
6. `getMessageProbeCount() == 1`, `getMessageProbeLastId() == ping_id`
7. `blunder_message_send(other, …)` — count stays `1` (cross-Object addressing)

## ADR 0017 + CONTEXT.md alignment (4.2)

| Doc term | Shipped API | Match |
|----------|-------------|-------|
| `Message.Send` | `Blunder.Message.Send` | yes |
| `Message.Register` | `Blunder.Message.Register` | yes |
| `OnMessage` | `Behaviour.OnMessage(MessageId, ReadOnlySpan<MessageArg>)` | yes |
| Directed to ObjectId, fan-out Behaviours | `MessageDispatch::send` + ScriptHost hook | yes |
| MVP Send from C#; native same path later | Test uses C-ABI send (delivery path shared) | yes (test exercises native sender) |

No doc drift fixes required.

## Commit

```
test: verify cross-Object Message reaches OnMessage
```

## Concerns

- Static `MessageCount` reset relies on fresh process per test run (same pattern as `ProbeBehaviour.TickCount`).
- `resolveProbeTickCount` name is overloaded — resolves all probe exports including message probes; rename optional follow-up.
