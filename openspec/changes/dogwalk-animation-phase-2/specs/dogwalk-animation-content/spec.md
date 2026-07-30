## ADDED Requirements

### Requirement: Weighted dual-track content under Play
Phase 2 content SHALL drive AnimationPlayer two-slot blend from Project C# (explicit slots + blendWeight) so idle↔walk can be continuously weighted. Gameplay Position move SHALL remain real-time every Tick; stepped facing SHALL continue to use PoseApplied with the dominant-slot playback clock.

#### Scenario: Weighted idle walk under Play
- **WHEN** Chocomel (or agreed subset) moves under Play with Phase 2 blend wired
- **THEN** visual locomotion can blend idle and walk by weight while position integrates every Tick

### Requirement: Phase 2 engineering gate
Phase 2 engineering validation SHALL demonstrate two-slot weighted blend, Crossfade (fade > 0), global TimeScale, and dominant-slot step sync on a test-rig (or equivalent) under Play and Edit scrub as applicable.

#### Scenario: Test-rig blend gate
- **WHEN** the Phase 2 engineering scene is exercised
- **THEN** dual-slot weight changes, Crossfade, and TimeScale visibly affect Skeleton pose / playback rate

### Requirement: Phase 2 Chocomel Play acceptance is Done criteria
Declaring DogWalk animation Phase 2 complete SHALL require Play acceptance with Chocomel (or an explicitly agreed subset): weighted idle↔walk (not hard-cut-only), perceptible TimeScale, real-time move, and stepped facing synced to the dominant slot. Test-rig-only success SHALL NOT alone mark Phase 2 Done.

#### Scenario: Chocomel weighted feel acceptance
- **WHEN** the Phase 2 acceptance scene with Chocomel (or agreed subset) is Played
- **THEN** idle↔walk blends by weight, TimeScale is perceptible, move is real-time, and facing steps with the dominant clip clock

### Requirement: Parallel with Phase 1 hard-cut gate
Phase 2 engine implementation MAY proceed while Phase 1 Chocomel hard-cut acceptance remains open. Phase 1 Done criteria (hard-cut idle↔walk, real-time move, stepped facing) SHALL remain in force and SHALL NOT be silently replaced by Phase 2 weighted acceptance alone.

#### Scenario: Both gates tracked
- **WHEN** Phase 2 work starts before Phase 1 Chocomel hard-cut closes
- **THEN** both Phase 1 hard-cut and Phase 2 weighted Done bars remain separately tracked
