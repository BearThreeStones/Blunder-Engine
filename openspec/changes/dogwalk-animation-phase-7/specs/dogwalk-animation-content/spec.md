## ADDED Requirements

### Requirement: Phase 7 engineering and Edit authorship gate
Phase 7 engineering validation SHALL demonstrate AnimationTree Canvas Asset round-trip (topology including transition edges and layout), condition-driven auto Travel (priority + hard-cut), Travel/Start coexistence, dual open paths, and Inspector dual-track topology edit — without requiring Behaviour Tick for Edit authorship.

#### Scenario: Harness gate
- **WHEN** the Phase 7 engineering harness and Edit authorship path are exercised
- **THEN** canvas Asset persistence and conditional auto Travel are observable under automation and/or Edit without DotNetHost Tick

### Requirement: Phase 7 lean Play bar
Declaring DogWalk animation Phase 7 complete SHALL require a lean Play acceptance: perceptible **automatic** state change driven by **transition conditions** (test rig or agreed subset OK). Full Chocomel/Pinda content parity SHALL NOT be the sole Play bar.

#### Scenario: Condition-driven Play feel
- **WHEN** Play advances with tree parameters/drives that satisfy a transition
- **THEN** the character's animation state changes automatically without that switch relying solely on an explicit script Travel for the observed transition

### Requirement: Parallel with Phase 1–6 content gates
Phase 7 MAY proceed while Phase 1–6 content Done gates remain open. Those earlier Done criteria SHALL remain in force and SHALL NOT be silently replaced by Phase 7 Canvas/transition acceptance alone.

#### Scenario: Earlier bars still tracked
- **WHEN** Phase 7 work starts before Phase 1–6 content gates close
- **THEN** earlier phase content bars and Phase 7 bars remain separately tracked
