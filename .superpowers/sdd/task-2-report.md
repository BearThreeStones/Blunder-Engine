# Task 2 Report: Instantiate Object bind + Behaviour slots

## Status

**DONE_WITH_CONCERNS**

## Summary

On `SceneInstance::instantiate`, entities with a non-empty `behaviours` list get an ObjectDB Object bound via `setEntityId`, with slots restored at persisted BehaviourIds and null peers (no DotNetHost Attach). `Object::restoreBehaviour` advances `m_next_behaviour_id` past the max restored id. Empty-behaviour entities do not create Objects. OpenSpec 2.1–2.3 marked `[x]`.

## Commits

- `0f2864f424126a0253f31ea024f9636bba4bdd25` — `feat(scene): bind Objects and restore Behaviour slots on load`

## Files changed

| Path | Action |
|------|--------|
| `engine/src/runtime/core/object/object.h` | Modified — `restoreBehaviour` |
| `engine/src/runtime/core/object/object.cpp` | Modified — restore slot + advance next id |
| `engine/src/runtime/core/object/object_db.h` | Modified — `findByEntityId` |
| `engine/src/runtime/core/object/object_db.cpp` | Modified — EntityId scan |
| `engine/src/runtime/function/scene/scene_instance.cpp` | Modified — bind Object + restore on instantiate |
| `engine/src/tests/scene_behaviour_instantiate_test.cpp` | Created — load JSON → instantiate without host |
| `engine/src/tests/CMakeLists.txt` | Modified — register test target |
| `openspec/changes/behaviour-serialization/tasks.md` | Modified — 2.1–2.3 `[x]` |

## TDD evidence

### RED

Added `scene_behaviour_instantiate_test` before instantiate bind existed.

```text
.\build\vs2026-debug\tests\Debug\scene_behaviour_instantiate_test.exe
FAIL actor has bound Object
1 failure(s)
```

Expected red (no Object for entity with behaviours).

### GREEN

Implemented `restoreBehaviour`, `ObjectDB::findByEntityId`, and instantiate bind/restore.

```text
cmake --build build/vs2026-debug --config Debug --target scene_behaviour_instantiate_test
.\build\vs2026-debug\tests\Debug\scene_behaviour_instantiate_test.exe  → exit 0
scene_behaviour_instantiate_test: all passed
```

Covers: Object bound to Actor EntityId; slots ids 1 and 3 with types; peers null; next id → 4 via `addBehaviour`; Prop without behaviours has no Object.

## Self-review

- **Correctness:** Restored ids preserved (not renumbered); invalid/duplicate ids skipped with warn; property bag not applied (Task 3).
- **Scope:** No DotNetHost Attach / mount (Task 3); no export (Task 4).
- **Teardown:** Test resets logger + ObjectDB before exit to avoid spdlog async-flush AV on process exit.

## Concerns

1. **EntityId is SceneInstance-local** — `ObjectDB::findByEntityId` is a process-global scan; multiple loaded SceneInstances can collide on EntityId values (pre-existing id scheme).
2. **No Object cleanup on `SceneInstance::clear()`** — re-instantiate can leave stale Objects in ObjectDB until an explicit clear (export path / Task 4 may need ownership tracking).
3. **Dirty working tree** — unrelated WIP left uncommitted; commit scoped to Object/scene instantiate + test + OpenSpec tasks.
