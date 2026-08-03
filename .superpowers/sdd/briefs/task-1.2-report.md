# Task 1.2 Report — Sync Group Fire per-member hard cut

## Status: DONE

## Commit

- `afc9a26` — Add Sync Group Fire with per-member hard-cut instructions.

## TDD evidence

### RED

- Extended `animation_sync_group_test.cpp` with 5 Fire test cases before implementation.
- Stub `fire()` returning `false` compiled; test run: **exit 1, 17 failures** (all Fire assertions).

### GREEN

- Implemented `SyncGroupFireInstruction`, `fire()`, `AnimationPlayer::snapPlay`, `seekPlayback`.
- Worktree build: `cmake --build build/vs2026-debug --target animation_sync_group_test --config Debug`
- **Result:** `animation_sync_group_test.exe` exit 0, 0 failures (14 tests total).

## What was implemented

### API surface

```cpp
struct SyncGroupFireInstruction {
  AnimationPlayer* player{nullptr};
  eastl::string clip_name;
  float seek_seconds{0.0f};
  bool has_seek{false};
};

bool AnimationSyncGroupService::fire(
    SyncGroupId id,
    const eastl::vector<SyncGroupFireInstruction>& instructions);
```

### AnimationPlayer helpers (Fire support)

- `snapPlay(name)` — hard snap: clears crossfade + dual-slot blend, begins clip at t=0.
- `seekPlayback(seconds)` — seek active clip (clamped); used when `has_seek` is set.

### Fire semantics

1. Validate group id, non-empty instructions, non-null players, group membership, clip resolvable (`resolveClipForName`).
2. Apply all instructions in one call (same logical moment): `snapPlayWithClip` then optional `seekPlayback`.
3. Default path is hard cut (no Crossfade ramp).

## Test coverage

| Test | Asserts |
|------|---------|
| `test_fire_heterogeneous_clips_hard_cut` | Two members fire different clip names; both playing at 0; not crossfading |
| `test_fire_with_seek` | Per-member optional seek positions applied |
| `test_fire_hard_cut_interrupts_crossfade` | Fire snaps out of mid-crossfade to new clip |
| `test_fire_from_mid_playback` | Fire replaces in-progress clips with new clips at 0 |
| `test_fire_validation` | Empty list, invalid id, null player, non-member, unknown clip all fail |
| `test_fire_atomic_on_resolve_failure` | Resolve failure leaves no member mutated (atomic apply) |

## Spec alignment

| Requirement | Status |
|-------------|--------|
| Fire accepts per-member `(AnimationPlayer, clipName[, seek])` | ✅ |
| Heterogeneous clip logical names | ✅ |
| Default Fire is hard cut (fade 0 / snap) | ✅ via `snapPlay` |
| Same logical moment start | ✅ batch apply in single `fire()` call |
| No CINE / C-ABI / Edit | ✅ |
| No same-name sugar (1.3) | ✅ |

## Concerns

1. ~~**Partial fire on mid-batch `snapPlay` failure**~~ — **Fixed** (see Atomic Fire fix below).
2. **`snapPlay` vs `play(name, 0)`** — `play` does not clear dual-slot state; Fire uses `snapPlay` so crossfade/slot blend is fully cleared. Task 1.4 may want `play(,0)` aligned with `snapPlay` for non-Sync paths.
3. **`seekPlayback` is new public API** — minimal; no dedicated unit test on AnimationPlayer (covered indirectly via sync group seek test).

## Atomic Fire fix (Important finding)

### Issue

`fire()` validated clip name→GUID mapping only, then applied via `snapPlay`. If a later member's clip could not be resolved (GUID mapped but no injected/resolver data), earlier members were already mutated while `fire()` returned `false`.

### Fix

1. **Pre-resolve in validation** — `resolveClipForName` checks full clip resolution before any player mutation.
2. **Pre-resolved batch apply** — resolved clips stored in a vector; apply uses `snapPlayWithClip` so apply does not re-resolve.
3. **New API** — `AnimationPlayer::resolveClipForName`, `AnimationPlayer::snapPlayWithClip`.

### TDD

- **RED:** `test_fire_atomic_on_resolve_failure` — player_a resolvable + mid-playback; player_b GUID-only (no clip data); fire fails; player_a clip/position unchanged.
- **GREEN:** `animation_sync_group_test.exe` exit 0, 0 failures (**15 tests**).

### Commit

- `db695ca` — Make Sync Group Fire atomic: pre-resolve clips before apply.


## Out of scope (confirmed not implemented)

- Same-name Fire sugar (task 1.3)
- Dedicated Crossfade-default-negative tests beyond hard-cut interrupt (task 1.4)
- CINE, C-ABI, Edit preview

## Next step

Task 1.3: optional same-name Fire sugar resolving via each player's name→GUID map.
