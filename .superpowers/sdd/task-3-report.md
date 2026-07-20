# Task 3 Report: Mount scene Behaviours when DotNetHost running

## Status

**DONE**

## Summary

When DotNetHost is running with a game assembly loaded, `mountSceneBehaviours` attaches managed peers onto restored Behaviour slots (stable BehaviourIds) and applies a minimal bool/number/string property bag. SceneSystem mounts after instantiate; host game-load remounts already-loaded instances (property bags skipped when Scene* is null). OpenSpec 3.1–3.3 marked `[x]`.

## Commits

- (pending) `feat(script): mount scene Behaviours when DotNetHost running`

## Files changed

| Path | Action |
|------|--------|
| `engine/src/runtime/function/script/scene_behaviour_mount.h` | Created — mount API |
| `engine/src/runtime/function/script/scene_behaviour_mount.cpp` | Created — Attach + property JSON apply |
| `engine/src/runtime/function/script/dotnet_host.h` / `.cpp` | Modified — existing-id attach, apply props, `hasGameAssembly`, `getProbePropertyOk` |
| `engine/managed/Blunder.ScriptHost/HostExports.cs` | Modified — mount onto existing id; `ApplyBehaviourProperties`; `GetProbePropertyOk` |
| `engine/src/runtime/function/scene/scene_system.cpp` | Modified — mount after instantiate |
| `engine/src/runtime/function/global/global_context.cpp` | Modified — remount loaded scenes after game load |
| `engine/src/tests/scene_behaviour_mount_test.cpp` | Created — process ABI + ProbeBehaviour Tick/props |
| `engine/src/tests/CMakeLists.txt` | Modified — register mount test |
| `engine/src/runtime/CMakeLists.txt` | Modified — mount sources |
| `engine/src/tests/fixtures/dotnet_host_game/ProbeBehaviour.cs` | Modified — property-bag fields + Ready capture |
| `openspec/changes/behaviour-serialization/tasks.md` | Modified — 3.1–3.3 `[x]` |

## API delivered

- `mountSceneBehaviours(SceneInstance&, DotNetHost&, const Scene*)`
- `DotNetHost::attachBehaviour` — non-zero `*out_id` mounts onto existing slot
- `DotNetHost::applyBehaviourProperties(object, id, utf8_json)`
- `DotNetHost::hasGameAssembly()` / `getProbePropertyOk()`

## TDD evidence

### RED

Stub `mountSceneBehaviours` (no-op). Ran `scene_behaviour_mount_test`:

```text
FAIL peer non-null after mount
getProbeTickCount=0 (expected 1)
FAIL managed Tick after scene mount
getProbePropertyOk=-1 (expected 1)
FAIL property bag applied to ProbeBehaviour
3 failure(s)
exit=1
```

### GREEN

Implemented Attach existing-id + mount helper + property apply.

```text
cmake --build build/vs2026-debug --config Debug --target scene_behaviour_mount_test
.\build\vs2026-debug\tests\Debug\scene_behaviour_mount_test.exe  → exit 0
  "scene_behaviour_mount_test OK"
editor_dotnet_host_test.exe / scene_behaviour_instantiate_test.exe → exit 0
```

## Self-review

- **Correctness:** Restored BehaviourId preserved (no second `add_behaviour`); peers null until mount; property bag skips unknown keys/types.
- **Scope:** No Inspector; no export (Task 4). Host not required for offline instantiate (Task 2 unchanged).
- **Remount:** After `loadGameAssembly`, remounts loaded SceneInstances without Scene* (no property re-apply).

## Concerns

1. **Property bags on remount-without-Scene** — when host starts after scenes load, peers attach from slot type names only; bag values are not re-applied unless mount is called with `Scene*`.
2. **Mount test needs `bin/Debug` on PATH** — large `engine_runtime`-linked exe needs staged ScriptHost/nethost DLLs (same as other scene tests).
3. **Dirty working tree** — unrelated WIP left uncommitted; commit scoped to mount path + test + OpenSpec tasks.
