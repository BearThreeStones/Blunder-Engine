# Task 2 Report: Instantiate Object bind + Behaviour slots

## Status

**DONE** (Important review findings fixed)

## Summary

On `SceneInstance::instantiate`, entities with a non-empty `behaviours` list get an ObjectDB Object bound via `setEntityId`, with slots restored at persisted BehaviourIds and null peers (no DotNetHost Attach). `Object::restoreBehaviour` advances `m_next_behaviour_id` past the max restored id. Empty-behaviour entities do not create Objects. OpenSpec 2.1–2.3 marked `[x]`.

Bound ObjectIds are tracked on `SceneInstance` and destroyed in `clear()` / destructor so re-instantiate cannot leave stale process-global Objects for `findByEntityId`.

## Commits

- `0f2864f424126a0253f31ea024f9636bba4bdd25` — `feat(scene): bind Objects and restore Behaviour slots on load`
- _(pending)_ — `fix(scene): destroy bound Objects on SceneInstance clear`

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

1. **EntityId is SceneInstance-local** — `ObjectDB::findByEntityId` remains a process-global scan; multiple *concurrently live* SceneInstances can still collide on EntityId values (pre-existing id scheme). Re-instantiate / clear paths are safe via per-instance ObjectId tracking. Multi-instance keying (`SceneInstance*` + EntityId) still deferred if needed.
2. ~~**No Object cleanup on `SceneInstance::clear()`**~~ — **Fixed** (see Review follow-up below).
3. **Dirty working tree** — unrelated WIP left uncommitted; commit scoped to Object/scene instantiate + test + OpenSpec tasks.

## Review follow-up (Important findings)

Addressed Task 2 review Important items:

1. `SceneInstance::clear()` (and destructor) destroys Objects tracked in `m_bound_object_ids`.
2. Instantiate records each created ObjectId; re-instantiate cannot leave stale bindings for `findByEntityId`.

### TDD evidence

**RED** (test only, before clear destroys Objects):

```text
.\build\vs2026-debug\tests\Debug\scene_behaviour_instantiate_test.exe
FAIL reinst clear destroys bound Object
FAIL reinst findByEntityId empty after clear
FAIL reinst first ObjectId invalidated after clear
FAIL reinst exactly one Object for EntityId
FAIL reinst one occupied Object after second instantiate
FAIL reinst second ObjectId differs from destroyed first
FAIL reinst third pass still one Object
FAIL reinst third pass one binding
8 failure(s)
exit=1
```

**GREEN** (track `m_bound_object_ids` + destroy on clear/dtor):

```text
cmake --build build/vs2026-debug --config Debug --target scene_behaviour_instantiate_test
.\build\vs2026-debug\tests\Debug\scene_behaviour_instantiate_test.exe  → exit 0
scene_behaviour_instantiate_test: all passed
```

Covers: instantiate → clear → instantiate → instantiate again; zero occupied Objects after clear; exactly one Object per EntityId; `findByEntityId` returns the live Object (not the destroyed id).

