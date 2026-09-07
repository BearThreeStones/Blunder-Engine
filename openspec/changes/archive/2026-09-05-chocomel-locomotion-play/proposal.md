## Why

Phase 4 AnimationTree runtime already samples an active tree (Travel / BlendSpace1D / PoseApplied). Test Project Play still drives locomotion with `PlayerMove` two-slot `AnimationPlayer` on cubes/`dogwalk_test_rig`, and **no Play scene hosts Chocomel**. Authors cannot validate the product pose path (active tree + scene-embedded BlendSpace1D) until a dedicated Chocomel scene and a Tree-only Behaviour exist. Independent Add2/OneShot clips are not in the project yet; this slice lands locomotion first without faking turn from walk.

## What Changes

- Add Test Project Behaviour **`Test.ChocomelLocomotion`**: Tick writes Position + BlendSpace1D scalar; Ready `Start("Locomotion")` on an **already active** AnimationTree; PoseApplied writes stepped visual yaw via `Object.Rotation`. Does **not** auto-activate the tree, call `AnimationPlayer.Play` / `SetSlot`, or Clip Play idle/walk.
- Leave **`Test.PlayerMove`** unchanged so `character_move` / `dogwalk_test_rig` / `phase3_sync_cine` keep two-slot proof.
- After the author builds `assets/Scenes/chocomel_locomotion.scene.asset` in the editor (mesh + Add Skeleton + Add AnimationTree + Clip Bindings `idle`/`walk` + script + static camera), patch **scene-embedded** topology: BlendSpace1D node `Locomotion` (idle@0, walk@1), StateMachine state `Locomotion`, `active` + `currentState`. No Tree Asset GUID. Omit Add2/OneShot keys.
- Do **not** change editor startup / `pick_test`. Do **not** hydrate Skeleton on instantiate (author Add… Skeleton in-session). Do **not** close Phase 4 content Done (visible Add2 turn + OneShot remain gated on real clips).

## Capabilities

### New Capabilities

- `chocomel-locomotion-play`: Test Project Play fixture for Chocomel BlendSpace1D locomotion (scene-embedded `Locomotion` graph, `ChocomelLocomotion` Behaviour, idle↔walk scalar, stepped facing). Explicitly **not** Phase 4 full content Done.

### Modified Capabilities

<!-- No `openspec/specs/` requirement changes. AnimationTree / Clip Play / csharp-behaviour kernels stay as archived. Phase 4 `dogwalk-animation-content` lives only on the unarchived phase-4 change and is not rewritten here. -->

## Impact

- **Test Project (not engine repo):** `Scripts/ChocomelLocomotion.cs`; later patch of `Assets/Scenes/chocomel_locomotion.scene.asset` `animationTree` JSON. Reuse `PlayerMoveMath` / `SteppedFacing`. Existing `Scripts.Tests` stay green.
- **Engine:** no AnimationTree / C-ABI / NativeAbi / hydrate-on-load work in this change.
- **Docs:** none required unless a one-line Play path note is useful; CONTEXT Phase 4 Done wording stays (Add2/OneShot still the milestone bar).
- **Depends on:** author in-editor assembly (hydration only on Add… Skeleton); Clip Bindings `idle`/`walk` → `LOOP-chocomel-idle` / `LOOP-chocomel-walk` (not `_1` duplicates).
