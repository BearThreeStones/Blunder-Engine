# Task 3.5 Fix — PoseApplied binding cleanup on destroy

**Issue:** `g_pose_applied_bindings` entries leaked when `blunder_object_destroy` ran with active PoseApplied listeners (Task 3.5 review, Important).

## Fix

`blunder_object_destroy` now clears the object's `AnimationPlayer` PoseApplied listeners and erases C-ABI bindings from `g_pose_applied_bindings` before `ObjectDB::destroy`.

## Test

`animation_player_c_abi_test.cpp`: destroy object with an active listener, then create a second object, subscribe, play, and advance — verifies no stale callbacks and no crash.

## Verification

```
animation_player_c_abi_test.exe  — exit 0
engine_c_abi_test.exe            — exit 0
native_abi_test.exe              — exit 0
```
