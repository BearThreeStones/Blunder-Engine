# Task 2.1 Report — Sync Fire → active tree OneShot

**Status:** DONE  
**Branch:** `feat/dogwalk-animation-phase-4`

## Summary

`AnimationSyncGroupService::fire` now detects when a member's co-located **AnimationTree** is **active** and routes the clip instruction through `requestOneShot` instead of `snapPlayWithClip` hard-cut. The tree remains active; Player sampling stays blocked. Members without an active tree keep Phase 3 hard-cut semantics.

## TDD evidence

1. **RED:** Four failing test groups added for active-tree OneShot, tree stays active, inactive-tree hard-cut regression, and Object-bound tree path.
2. **GREEN:** `animation_sync_group_test.exe` exits 0 (all groups including new task 2.1 tests).

### New tests

| Test | Proves |
|------|--------|
| `test_fire_active_tree_member_applies_oneshot` | Fire on active-tree member activates OneShot, samples trip pose, does not hard-cut Player |
| `test_fire_active_tree_does_not_deactivate_tree` | Tree stays active and bound after Fire |
| `test_fire_inactive_tree_member_still_hard_cut` | Inactive tree → Phase 3 snap Play (no OneShot) |
| `test_fire_active_tree_via_object_binding` | Object `ensureAnimationTree` path wires player↔tree; Fire applies OneShot |

## Production changes

| File | Change |
|------|--------|
| `animation_sync_group.cpp` | `fire()` branches: active tree → `requestOneShot`; else `snapPlayWithClip` |
| `animation_player.h/.cpp` | `bindAnimationTree` / `getAnimationTree` back-pointer |
| `animation_tree.cpp` | `bindAnimationPlayer` sets/clears player back-pointer |
| `object.cpp` | Clears player tree binding when AnimationTree removed |
| `animation_sync_group_test.cpp` | Four task 2.1 tests |
| `tasks.md` | 2.1 marked `[x]` |

## Test command

```powershell
cmake --build build/vs2026-debug --target animation_sync_group_test --config Debug
build/vs2026-debug/engine/src/tests/Debug/animation_sync_group_test.exe
```

## Concerns

- **Seek on active-tree Fire:** `has_seek` is ignored for OneShot members (no `setOneShotTime` API yet); hard-cut members still honor seek.
- **Mixed groups** (tree + no-tree) covered at unit level; task 2.2/2.3 add explicit mixed-alignment tests.
- **Managed API** (task 5.2) inherits semantics once C-ABI Fire path uses the same service.
