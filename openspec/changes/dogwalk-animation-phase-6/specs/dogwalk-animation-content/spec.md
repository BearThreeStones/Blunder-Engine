## ADDED Requirements

### Requirement: Phase 6 modifier content Done
Declaring DogWalk animation Phase 6 complete SHALL require the engineering gate for PaperMouth, SkeletonAttachModifier, and configurable LookAt (including scene persistence / Inspector) **plus** lean Edit/Play bars demonstrating visible mouth open, child prop follow, and aim change. Leash, Tree canvas, and full Pinda parity SHALL NOT be the sole acceptance bars.

#### Scenario: Engineering without Play feel
- **WHEN** engineering tests pass but lean Play/Edit feel bars for the three modifiers are open
- **THEN** Phase 6 Done remains incomplete

### Requirement: Parallel with Phase 1–5 content gates
Phase 6 engine work MAY proceed while Phase 1–5 content Done gates remain open. Those earlier Done criteria SHALL remain in force and SHALL NOT be silently replaced by Phase 6 acceptance alone.

#### Scenario: Earlier bars still tracked
- **WHEN** Phase 6 work starts before Phase 1–5 content gates close
- **THEN** earlier and Phase 6 Done bars remain separately tracked
