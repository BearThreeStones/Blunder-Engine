## Context

See proposal.md for motivation. AnimationTree already samples StateMachine / BlendSpace / OneShot / Add2 through Pipeline evaluate. `requestOneShot` latches on inactive trees; Sync Group Fire fails inactive members. Sampler holds the last key when time is past the clip. Animation Window `stop` already `clearOneShot` + `seekRuler(0)`. Scene deserialize restores authored OneShot *slot* clip, not live OneShot occupancy. NativeAbi is a sequential function-pointer table at C-ABI v11.

## Goals / Non-Goals

**Goals:**

- Runtime Clip Play override on `AnimationTree` with the grilled sample stack
- C-ABI + NativeAbi append + `AnimationTree.Play` in Blunder.Api
- Window Stop/rebind clears override; ruler reads override when Fire is empty
- Engine tests covering success, hold, Travel clear, transition suspend, Fire stack, failures, non-persistence

**Non-Goals:**

- Fade / unified BlendSpec types
- Window Clip Play chrome
- Changing `requestOneShot` inactive latch behavior
- DogWalk content Behaviours

## Decisions

1. **Override flag on AnimationTree, not a synthetic StateMachine state**  
   Remember `m_current_state_name`; skip `evaluateTransitions` while override is set; `sampleBaseOntoSkeleton` samples the Play clip unless OneShot is occupying.  
   *Alternatives:* Canvas-visible synthetic state (rejected in grill); deactivate SM.

2. **OneShot sample order stays first**  
   Existing `m_oneshot_active` early-return in `sampleBaseOntoSkeleton` and `rulerClipName` remains the Fire cover. Clip Play is the base under that. Advance still increments both OneShot time and Clip Play time.  
   *Alternatives:* Pause Clip Play clock during Fire (extra mode).

3. **Travel / Start always clear override before applying the state**  
   Even Travel to the current state name resumes SM sampling. Start still seeks the state clock to 0 after travel.  
   *Alternatives:* Travel to current state as no-op (would leave Hit stuck).

4. **C-ABI `blunder_animation_tree_play` + NativeAbi append; version 12**  
   Same shape as `request_one_shot` (ObjectId, UTF-8 logical name). Append pointer at end of `BlunderNativeAbi` / `NativeAbi.cs`. C# façade `Play(string clipName)`.  
   *Alternatives:* `clip_play` method name only in C# (product term is Clip Play anyway); reuse `animation_player_play` (retired product path).

5. **Preview clear is a tree method next to `clearOneShot`**  
   Window `stop` and rebind Stop call it. Not a C-ABI product Play-clear in v1 (Travel/Start/Stop preview are the clears).  
   *Alternatives:* C-ABI `clear_clip_play` for tests only — native tests can call C++.

6. **Do not add override to scene export / instance overrides / ClassDB properties**  
   Live OneShot occupancy is already not the deserialized slot. Clip Play has no authored slot.  
   *Alternatives:* Persist for Play-mode restore (rejected in grill).

## Risks / Trade-offs

- [Inactive RequestOneShot still latches; Clip Play fails] → Document as intentional; tests for both
- [Travel during Fire clears override while Fire still occupies] → After Fire ends, pose is SM not Hit; scripts that Travel mid-Fire must accept that
- [NativeAbi layout break for out-of-tree hosts] → Version 12 completeness check; append-only fields
- [Edit tests may not have a Clip Play C-ABI if only C++ `clipPlay`] → Engine unit tests call C++; add C-ABI test beside existing tree ABI checks

## Migration Plan

1. Runtime override + sample/advance/Travel/Start
2. Tests (TDD on `animation_tree_test`)
3. C-ABI / NativeAbi / Blunder.Api / NativeAbiTests
4. Window Stop + ruler
5. Confirm scene save/load does not write override
6. Rollback = revert the change; no scene format migration

## Open Questions

None — C-ABI symbol is `blunder_animation_tree_play` (managed `Play`).
