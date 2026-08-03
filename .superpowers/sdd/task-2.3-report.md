# Task 2.3 Report — Mixed group aligns at same logical moment

**Status:** DONE  
**Branch:** `feat/dogwalk-animation-phase-4`

## Summary

A single Sync Group `fire` call on a **mixed group** (character with active AnimationTree + prop with no tree) applies **OneShot** on the character (`SYNC-attach`) and **hard-cut Play** on the prop (`SYNC-prop`) in one batch. Both co-located skeletons receive starting poses at the fire moment; subsequent `advance` on each member progresses independently from that shared logical instant.

## TDD evidence

1. **RED:** `test_fire_mixed_group_aligns_same_logical_moment` added — spec scenario: character OneShot `SYNC-attach` + prop hard-cut `SYNC-prop` same Fire.
2. **GREEN:** `animation_sync_group_test.exe` exits 0 (routing from task 2.1 handles per-member branching in one `fire()` loop).

### New test

| Test | Proves |
|------|--------|
| `test_fire_mixed_group_aligns_same_logical_moment` | Single Fire: character tree stays active + OneShot samples attach pose; prop hard-cuts `SYNC-prop` at t=0; both skeletons posed; joint advance continues from fire moment |

### Assertions at fire moment

| Member | Route | Post-fire state |
|--------|-------|-----------------|
| Character (active tree) | `requestOneShot("SYNC-attach")` | Tree active, OneShot active, Player blocked, locomotion state unchanged, attach pose on skeleton |
| Prop (no tree) | `snapPlayWithClip("SYNC-prop")` | Playing, position 0, not crossfading, start pose on skeleton |

### Post-advance (0.25s)

- Character OneShot still active; attach pose held (constant clip)
- Prop playback at 0.25s; linear clip pose interpolated

## Production changes

| File | Change |
|------|--------|
| `animation_sync_group_test.cpp` | `test_fire_mixed_group_aligns_same_logical_moment` |
| `tasks.md` | 2.3 marked `[x]` |

No runtime code changes — per-member routing in `animation_sync_group.cpp::fire()` already processes heterogeneous instructions in one atomic batch.

## Test command

```powershell
cmake --build build/vs2026-debug --target animation_sync_group_test --config Debug
build/vs2026-debug/engine/src/tests/Debug/animation_sync_group_test.exe
```

## Concerns

- **Atomic failure:** If prop clip fails to resolve, entire `fire` fails before any member changes (existing `test_fire_atomic_on_resolve_failure`).
- **Seek asymmetry:** `has_seek` applies to hard-cut members only; OneShot members ignore seek until `setOneShotTime` exists.
- **Managed API:** Task 5.2 must expose same per-member routing through C-ABI / Blunder.Api.
