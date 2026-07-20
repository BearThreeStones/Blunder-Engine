# Task 4 Report: Export Behaviours from bound Objects

## Status

**DONE**

## Summary

`SceneInstance::exportToScene` now writes each entity’s Behaviour list (type + id; empty property bag) from the SceneInstance-tracked bound Object. Soft-deleted entities remain omitted, so their behaviours are not written. OpenSpec 4.1–4.3 marked `[x]`.

## Commits

- (pending) `feat(scene): export Behaviours from bound Objects`

## Files changed

| Path | Action |
|------|--------|
| `engine/src/runtime/function/scene/scene_instance.cpp` | Modified — export behaviours via `m_bound_object_ids` |
| `engine/src/tests/scene_behaviour_export_test.cpp` | Created — live Object source + tombstone omission |
| `engine/src/tests/CMakeLists.txt` | Modified — register `scene_behaviour_export_test` |
| `openspec/changes/behaviour-serialization/tasks.md` | Modified — 4.1–4.3 `[x]` |

## TDD evidence

### RED

```text
.\build\vs2026-debug\tests\Debug\scene_behaviour_export_test.exe
FAIL Actor behaviours from Object (3 slots)
1 failure(s)
```

Expected: export still dropped behaviours (empty list) despite bound Object slots + live `addBehaviour`.

### GREEN

```text
cmake --build build/vs2026-debug --config Debug --target scene_behaviour_export_test
.\build\vs2026-debug\tests\Debug\scene_behaviour_export_test.exe  → exit 0
scene_behaviour_export_test: all passed

.\build\vs2026-debug\tests\Debug\scene_behaviour_instantiate_test.exe  → exit 0
.\build\vs2026-debug\tests\Debug\scene_serializer_test.exe  → exit 0
```

Covers: export reads Object slots (ids 1,3 + live-added id 4); Prop has no behaviours; soft-deleted Actor omitted entirely.

## Concerns

1. **Property bag not on Object** — export writes empty `properties`; round-trip of bag values still depends on Task 3 mount applying declarations at load, not Object storage. Full Object→property export is out of this slice.
2. **`scene_soft_delete_test` link** — rebuild failed (`blunder_native_abi_fill_from_process` unresolved) because that target does not link `blunder_engine_c_static`; pre-existing vs current `global_context`. Tombstone+behaviour omission is covered by `scene_behaviour_export_test`.
3. **Dirty working tree** — unrelated WIP left uncommitted; commit scoped to export + test + OpenSpec tasks.
