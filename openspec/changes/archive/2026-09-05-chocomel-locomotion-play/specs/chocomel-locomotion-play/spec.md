## Purpose

Test Project Play acceptance for Chocomel locomotion on an active scene-embedded AnimationTree BlendSpace1D, without claiming Phase 4 full content Done (Add2 turn and OneShot remain gated on independent clips).

## ADDED Requirements

### Requirement: Dedicated Chocomel locomotion Play scene
The Test Project SHALL provide a Play scene at virtual path `assets/Scenes/chocomel_locomotion.scene.asset`. Editor default startup SHALL remain `pick_test` (or the current default). Existing scenes `character_move`, `dogwalk_test_rig`, and `phase3_sync_cine` SHALL keep their current Behaviours and SHALL NOT be rewritten onto AnimationTree locomotion.

#### Scenario: Scene exists without stealing startup
- **WHEN** the Test Project is opened in the editor without an explicit scene override
- **THEN** startup SHALL NOT load `chocomel_locomotion.scene.asset` as the default scene

#### Scenario: Locomotion scene is openable
- **WHEN** the author opens `assets/Scenes/chocomel_locomotion.scene.asset`
- **THEN** the active instance SHALL include a character entity with a Mesh, a Skeleton, an active AnimationTree, Clip Bindings named `idle` and `walk`, and a locomotion Behaviour

### Requirement: Scene-embedded Locomotion BlendSpace1D
The locomotion scene entity SHALL persist AnimationTree topology in the scene document (no AnimationTree Asset GUID required). The graph SHALL include a BlendSpace1D node named `Locomotion` with Clip Binding points `idle` at scalar 0 and `walk` at scalar 1, and a StateMachine state named `Locomotion` whose playback is that BlendSpace1D. The tree SHALL be active with current state `Locomotion`. Add2 and OneShot authored slots SHALL be omitted until independent clips exist.

#### Scenario: Reload restores locomotion graph
- **WHEN** the locomotion scene is saved with the embedded `Locomotion` BlendSpace1D and state
- **AND** the scene is instantiated
- **THEN** Travel/Start to `Locomotion` SHALL sample that BlendSpace1D
- **AND** setting the `Locomotion` BlendSpace scalar SHALL blend between `idle` and `walk` by those authored points

#### Scenario: No Tree Asset required
- **WHEN** the locomotion entity’s AnimationTree Asset GUID is empty
- **THEN** instantiate SHALL apply the scene-embedded BlendSpace points and states (not instance-override-only)

#### Scenario: Add2 and OneShot omitted
- **WHEN** the locomotion scene is inspected on disk after this change
- **THEN** it SHALL NOT bind walk (or idle) as a fake Add2 turn clip
- **AND** it SHALL NOT require a OneShot clip for this slice to be accepted

### Requirement: Tree-only locomotion Behaviour
The locomotion entity SHALL host a Behaviour type `Test.ChocomelLocomotion` that drives pose through the co-located active AnimationTree. On Ready it SHALL Start state `Locomotion` without activating an inactive tree. Each Tick it SHALL move the Object from gameplay move input and set the `Locomotion` BlendSpace1D scalar from stick magnitude (0 at rest, 1 at full stick). It SHALL NOT write AnimationPlayer two-slot blend, SHALL NOT AnimationPlayer.Play locomotion clips, and SHALL NOT Clip Play idle or walk.

#### Scenario: Stick drives BlendSpace scalar
- **WHEN** Play is running on the locomotion scene with an active tree
- **AND** move input magnitude goes from rest to full
- **THEN** the `Locomotion` BlendSpace scalar SHALL move from 0 toward 1
- **AND** the sampled base SHALL change from idle-dominant to walk-dominant

#### Scenario: Inactive tree is not auto-activated
- **WHEN** Ready runs and the co-located AnimationTree is not active
- **THEN** the Behaviour SHALL NOT set the tree active
- **AND** it SHALL NOT fall back to AnimationPlayer two-slot Play

#### Scenario: Existing PlayerMove scenes still two-slot
- **WHEN** `character_move` Play runs
- **THEN** `Test.PlayerMove` SHALL still be the movement Behaviour
- **AND** it SHALL NOT require an active AnimationTree

### Requirement: Stepped visual facing on PoseApplied
Gameplay yaw SHALL track move input every Tick. Visual yaw SHALL update only on Animation steps (PoseApplied plus the base dominant-clip playback clock). Visual rotation SHALL be written as Object Rotation around world +Z, with mesh forward +Y (zero yaw when moving +Y).

#### Scenario: Facing steps with pose
- **WHEN** the character moves with a non-zero stick
- **THEN** gameplay yaw tracks the stick in real time
- **AND** Object Rotation SHALL change on Animation steps, not every Tick

### Requirement: This slice does not close Phase 4 content Done
Accepting this locomotion Play fixture SHALL NOT by itself declare DogWalk animation Phase 4 content complete. Visible additive turn and OneShot return-to-base SHALL remain open until independent clips exist.

#### Scenario: Locomotion Play is a subset
- **WHEN** idle↔walk BlendSpace motion and stepped facing are perceptible in Play
- **THEN** Phase 4 content Done (additive turn + OneShot) SHALL remain separately tracked
