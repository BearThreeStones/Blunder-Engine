## 1. ChocomelLocomotion Behaviour

- [x] 1.1 Add `E:\Blunder Projects\Test\Scripts\ChocomelLocomotion.cs` (`namespace Test`, type `ChocomelLocomotion`). Public fields: `Speed`, `FrameTime`, `FacingSlices`, `TimeScale` only (same defaults as `PlayerMove`).
- [x] 1.2 Ready: construct `SteppedFacing` + `AnimationStepClock`; `EnsureAnimationTree` + `EnsureAnimationPlayer`; write `TimeScale` on the Player; subscribe `PoseApplied`; if tree `Active`, `Start("Locomotion")`; if not active, log and skip (do not set `Active`). No `SetSlot`, `BlendWeight`, `AnimationPlayer.Play`, `AnimationTree.Play`, IdleClip/WalkClip.
- [x] 1.3 Tick: `Input.GetMove` → `PlayerMoveMath.Apply` on `Object.Position`; `SetBlendSpaceScalar("Locomotion", PlayerMoveMath.ComputeBlendWeight(move))`; gameplay yaw via `SteppedFacing.MoveToYawDegrees` when stick non-zero.
- [x] 1.4 PoseApplied: `AnimationStepClock.ConsumeStep(player.PlaybackPosition)` then `ApplyStepFromGameplay` and `Object.Rotation = Quat.AngleAxis(visualYawRad, Vec3.UnitZ)`.
- [x] 1.5 Leave `PlayerMove.cs` and existing Play scenes (`character_move`, `dogwalk_test_rig`, `phase3_sync_cine`, `pick_test`) unchanged.

## 2. Scripts build

- [x] 2.1 `dotnet build` Test `Scripts/Test.csproj` (and `Scripts.Tests`) so `Test.ChocomelLocomotion` is loadable. No new test types required.

## 3. Author scene (human, then agent waits)

- [x] 3.1 Author creates `Assets/Scenes/chocomel_locomotion.scene.asset`: one Chocomel entity (mesh `Chocomel`, Add Skeleton, Add AnimationTree, Clip Bindings `idle`/`walk` → `LOOP-chocomel-idle` / `LOOP-chocomel-walk` not `_1`), attach `Test.ChocomelLocomotion`, static camera/light copied from `character_move`. Tree Asset GUID empty; tree left active. Save and tell the implementer the file is on disk.

## 4. Embedded topology patch

- [x] 4.1 After the scene file exists, patch that entity’s `animationTree` to: `active: true`, `currentState: "Locomotion"`, `baseBlendSpaceNode: "Locomotion"`, BlendSpace1D `Locomotion` points `idle@0` / `walk@1`, initial `scalar: 0`, state `Locomotion` kind `blendSpace1D`. Omit `add2` and `oneShotClip`. Do not set `assetGuid`. Keep author Clip Bindings.

## 5. Validation

- [x] 5.1 Run Test `Scripts.Tests` (PlayerMoveMath / SteppedFacing still pass).
- [x] 5.2 Manual Play: same editor session, open `chocomel_locomotion`, Play — rest idle, stick walk blend, stepped facing, no two-slot. Do not treat post-reload T-pose as a fail of this change (empty Skeleton until a later hydrate-on-load).
