## ADDED Requirements

### Requirement: Phase 4 Chocomel-subset Play acceptance
Declaring DogWalk animation Phase 4 complete SHALL require Play acceptance of Chocomel (or an agreed subset) driven by AnimationTree: perceptible BlendSpace1D speed-like motion, **visible additive turn** overlay (exact Godot turn clip names NOT required), OneShot return-to-base, and stepped facing still following the base dominant-clip clock. BlendSpace2D / full Pinda parity SHALL NOT be the Phase 4 acceptance bar.

#### Scenario: Chocomel-subset feel
- **WHEN** the Phase 4 acceptance content is Played
- **THEN** locomotion BlendSpace motion and additive turn are perceptible, OneShot returns to base, and stepped facing remains tied to the base dominant clock

### Requirement: Phase 4 engineering gate
Phase 4 engineering validation SHALL demonstrate StateMachine Travel/Start, BlendSpace1D scalar blend, OneShot (including Sync Fire→active tree), Add2 overlay, exclusive active sampling, PoseApplied/dominant-clip clock, TimeScale, and Edit scrub without Behaviour Tick.

#### Scenario: Harness gate
- **WHEN** the Phase 4 engineering harness is exercised
- **THEN** tree sample-stack behaviours above are observable under automation and/or Edit preview

### Requirement: Parallel with Phase 1–3 content gates
Phase 4 engine implementation MAY proceed while Phase 1–3 Chocomel / weighted / mini SYNC-CINE content Done gates remain open. Those earlier Done criteria SHALL remain in force and SHALL NOT be silently replaced by Phase 4 AnimationTree acceptance alone.

#### Scenario: Earlier bars still tracked
- **WHEN** Phase 4 work starts before Phase 1–3 content gates close
- **THEN** Phase 1 hard-cut, Phase 2 weighted, Phase 3 mini SYNC/CINE, and Phase 4 AnimationTree Done bars remain separately tracked
