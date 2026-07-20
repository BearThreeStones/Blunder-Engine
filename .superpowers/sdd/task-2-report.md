# Task 2 Report: LifecycleDispatch ticks every Behaviour peer

**Status:** DONE  
**Branch:** `feat/dotnet-host-mvp`  
**Date:** 2026-07-20

## Summary

`LifecycleDispatch::invokeTick` / `invokeReady` now walk every Behaviour Script Peer in list order (skipping nulls). `BehaviourSlot::ready_invoked` gates Ready so it fires once per peer until the peer is reset. `ObjectDB::forEach` visits occupied Objects for later world Tick (Task 8).

## TDD evidence

1. Extended `behaviour_list_test` with multi-peer Tick, null-skip, Ready-once, peer-reset Ready, and `ObjectDB::forEach` assertions.
2. First compile failed on missing `ObjectDB::forEach` (red).
3. After adding `forEach` only: runtime FAIL `both peers ticked` (+ Ready assertions) — confirmed old single-peer path.
4. Implemented multi-peer dispatch + `ready_invoked`; all targeted tests exit 0.

## Changes

| File | Change |
|------|--------|
| `lifecycle.cpp` | Walk `getBehaviourCount` / peers; skip null; Ready gated by `ready_invoked` |
| `lifecycle.h` | Comment documenting multi-peer + Ready-once semantics |
| `object.h` / `object.cpp` | `ready_invoked` on `BehaviourSlot`; `isBehaviourReadyInvoked` / `markBehaviourReadyInvoked`; reset on peer set/clear |
| `object_db.h` / `object_db.cpp` | `ObjectVisitorFn` + `ObjectDB::forEach` |
| `behaviour_list_test.cpp` | Multi-peer Tick/Ready/`forEach` coverage |
| `openspec/.../tasks.md` | Marked 2.1–2.4 done |

## Tests run

```
behaviour_list_test.exe       exit 0
ptrcall_lifecycle_test.exe    exit 0
engine_c_abi_test.exe         exit 0
```

## Out of scope (deferred)

- DotNetHost / C-ABI Behaviour APIs (Task 3+)
- Engine-frame world Tick wiring via `forEach` (Task 8)

## Concerns

- `ObjectDB::forEach` uses a plain function-pointer visitor (`ObjectVisitorFn`), not a template/lambda — Task 8 may want a thin wrapper or `eastl::function` overload for capture-friendly world Tick.
- `isBehaviourReadyInvoked` / `markBehaviourReadyInvoked` are public; could later be narrowed (friend `LifecycleDispatch`) if we want a smaller Object surface.
- Setting the same peer pointer again still resets `ready_invoked` (intentional “peer set clears Ready”); attach/re-attach semantics for managed hosts should stay aware of that.
