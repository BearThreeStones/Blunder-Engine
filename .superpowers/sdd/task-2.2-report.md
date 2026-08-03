# Task 2.2 Report — Fire on no-tree member remains Phase 3 hard-cut Play

**Status:** DONE  
**Branch:** `feat/dogwalk-animation-phase-4`

## Summary

Sync Group `fire` on a member that **never had** an AnimationTree continues Phase 3 hard-cut `snapPlayWithClip` semantics: clears crossfade and dual-slot blend, resets playback to clip start, and samples the bound skeleton immediately. Task 2.1 already covered inactive-tree members; this task adds an explicit no-tree path with strengthened assertions.

## TDD evidence

1. **RED:** `test_fire_no_tree_member_phase3_hard_cut` added — dual-slot weighted blend before Fire, player with `getAnimationTree() == nullptr`.
2. **GREEN:** `animation_sync_group_test.exe` exits 0 (no production changes required; routing from task 2.1 `else` branch already handles `tree == nullptr`).

### New test

| Test | Proves |
|------|--------|
| `test_fire_no_tree_member_phase3_hard_cut` | No-tree member: Fire clears dual-slot blend, hard-cuts `SYNC-attach`, samples skeleton pose, never touches OneShot |

### Strengthened assertions vs 2.1 inactive-tree test

- `getAnimationTree() == nullptr` (never bound, not merely inactive)
- Dual-slot blend active before Fire → slots and blend weight cleared after
- Skeleton translation pose matches fired clip at time zero
- Crossfade cleared, playback position reset

## Production changes

| File | Change |
|------|--------|
| `animation_sync_group_test.cpp` | `test_fire_no_tree_member_phase3_hard_cut` |
| `tasks.md` | 2.2 marked `[x]` |

No runtime code changes — existing `fire()` branch:

```cpp
if (tree != nullptr && tree->isActive()) { requestOneShot(...); }
else { snapPlayWithClip(...); }
```

covers `tree == nullptr`.

## Test command

```powershell
cmake --build build/vs2026-debug --target animation_sync_group_test --config Debug
build/vs2026-debug/engine/src/tests/Debug/animation_sync_group_test.exe
```

## Concerns

- **Inactive vs no-tree:** Both route through the same hard-cut path; only test setup differs.
- **Seek on hard-cut Fire:** `has_seek` honored for no-tree members (unchanged from Phase 3).
