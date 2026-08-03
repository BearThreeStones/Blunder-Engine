# Task 1.3 Report â€?same-name Fire sugar

## Status: DONE

## Commit

- `65f82b1` â€?Add Sync Group fireSameName convenience sugar.

## TDD evidence

### RED

- Added 3 `fireSameName` test cases to `animation_sync_group_test.cpp` before implementation.
- Without API/impl, compile fails on `service.fireSameName(...)`.

### GREEN

- Implemented `fireSameName(groupId, clipName)` and `fireSameName(groupId, clipName, seekSeconds)` as thin sugar over existing `fire()`.
- Build: `cmake --build build/vs2026-debug --target animation_sync_group_test --config Debug`
- **Result:** `animation_sync_group_test.exe` exit 0, 0 failures (**18 tests** total, +3 from task 1.2).

## What was implemented

### API surface

```cpp
/// Fire the same clip logical name on every group member (each resolves via own map).
bool fireSameName(SyncGroupId id, const eastl::string& clip_name);
/// Same with seek applied to all members.
bool fireSameName(SyncGroupId id, const eastl::string& clip_name,
                  float seek_seconds);
```

### Semantics

1. Validates group id, non-empty clip name, group exists, group has at least one member.
2. Builds one `SyncGroupFireInstruction` per member (stable insertion order) with the shared logical clip name.
3. Delegates to `fire()` â€?each member resolves the name through its own nameâ†’GUID map; atomic pre-resolve/apply behavior inherited from task 1.2.

## Test coverage

| Test | Asserts |
|------|---------|
| `test_fire_same_name_resolves_per_member_map` | Two members share logical name "walk" but different GUIDs; both play at t=0 |
| `test_fire_same_name_with_seek` | Shared seek position applied to all members |
| `test_fire_same_name_validation` | Invalid id, empty name, empty group fail; resolve failure is atomic |

## Spec alignment

| Requirement | Status |
|-------------|--------|
| Same logical name convenience sugar (optional, not only path) | âœ?|
| Each player resolves via own nameâ†’GUID map | âœ?via delegated `fire()` |
| Builds on existing per-member `fire()` | âœ?|
| Optional seek | âœ?second overload |
| No CINE | âœ?|

## Concerns

1. **Empty group fails** â€?`fireSameName` rejects groups with zero members (cannot build non-empty instruction list). Callers must join members first; consistent with `fire()` requiring non-empty instructions.
2. **No subset fire** â€?sugar always fires all members; heterogeneous per-member names still require explicit `fire(instructions)`.
3. **Duplicated validation in both overloads** â€?minor; could factor a private helper if more sugar overloads appear.

## Out of scope (confirmed not implemented)

- CINE, C-ABI, Edit preview
- Crossfade-default-negative tests (task 1.4)

## Next step

Task 1.4: Crossfade-default-negative / hard-cut alignment tests.
