## ADDED Requirements

### Requirement: Phase 5 per-gate lean Play acceptance
Declaring DogWalk animation Phase 5 complete SHALL require all three engineering gates **and** lean Play bars: **(A)** visible post-pose modifier effect + observable method dispatch; **(C)** perceptible BlendSpace2D blend (Pinda subset or test field OK — full Pinda not required); **(D)** Asset reference + Inspector edit + instance override without Behaviour Tick. Visual canvas and full leash/paper-mouth parity SHALL NOT be the sole acceptance bars.

#### Scenario: Three bars tracked
- **WHEN** Phase 5 engineering gates pass but a lean Play bar for one gate is open
- **THEN** Phase 5 Done remains incomplete for that gate's Play bar

### Requirement: Phase 5 engineering gates
Phase 5 engineering validation SHALL demonstrate Gate A (modifier chain + method dispatch), Gate C (BlendSpace2D barycentric), Gate D (Tree Asset + Inspector + overrides), and Edit scrub of A/C/D without Behaviour Tick.

#### Scenario: Harness gates
- **WHEN** Phase 5 engineering harnesses are exercised
- **THEN** A/C/D behaviours above are observable under automation and/or Edit preview

### Requirement: Parallel with Phase 1–4 content gates
Phase 5 engine work MAY proceed while Phase 1–4 content Done gates remain open. Those earlier Done criteria SHALL remain in force and SHALL NOT be silently replaced by Phase 5 acceptance alone.

#### Scenario: Earlier bars still tracked
- **WHEN** Phase 5 work starts before Phase 1–4 content gates close
- **THEN** earlier and Phase 5 Done bars remain separately tracked
