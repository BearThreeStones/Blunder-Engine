## ADDED Requirements

### Requirement: ValueSlicer is project C# policy
Stepped visual quantization (ValueSlicer with hysteresis) SHALL live in Project C# (utility and/or Behaviour), not as a required engine ClassDB type for Phase 1. Gameplay motion and input SHALL remain real-time; sliced values SHALL NOT overwrite the authoritative gameplay floats in place.

#### Scenario: Facing uses sliced visual
- **WHEN** Chocomel (or test character) moves under Play with step-synced facing
- **THEN** visual yaw updates on Animation step boundaries via C# ValueSlicer while position integrates every Tick

### Requirement: Animation step sync from PoseApplied
Phase 1 content SHALL detect Animation steps using PoseApplied (or post-sample timing) plus playback position (e.g. frametime modulo for on-2s), not an engine-builtin AnimationStep event.

#### Scenario: Step-aligned update
- **WHEN** a stepped clip plays and playback crosses a step boundary
- **THEN** step-synced visual updates run in response to that timing under Play

### Requirement: Engineering gate with test rig
Phase 1 engineering validation SHALL include a minimal skinned mesh with at least idle and walk AnimationClips, AnimationPlayer hard cuts, and visible deformation under Play and Edit preview.

#### Scenario: Test rig idle walk
- **WHEN** the test-rig scene is Played
- **THEN** the character can hard-cut between idle and walk and the skinned mesh deforms

### Requirement: Chocomel Play acceptance is Done criteria
Declaring DogWalk animation Phase 1 complete SHALL require Play acceptance with Chocomel (or an explicitly agreed subset): idle↔walk hard cut, real-time horizontal move, and stepped facing. Test-rig-only success SHALL NOT alone mark the milestone Done.

#### Scenario: Chocomel feel acceptance
- **WHEN** the acceptance scene with Chocomel (or agreed subset) is Played
- **THEN** idle↔walk hard cuts work, move responds in real time, and facing updates in a Stepped manner synced to animation timing
