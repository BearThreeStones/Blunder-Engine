## ADDED Requirements

### Requirement: Phase 3 mini multi-Object Play acceptance
Declaring DogWalk animation Phase 3 complete SHALL require Play acceptance of a DogWalk-style mini scene: at least one character AnimationPlayer and one prop/partner AnimationPlayer start together via Sync Group, a CINE segment Enter/End returns control to gameplay, and simplified props are allowed. Full Chocomel/Pinda ending sequences SHALL NOT be the sole acceptance bar.

#### Scenario: Mini sync + handoff
- **WHEN** the Phase 3 acceptance scene is Played
- **THEN** character and prop/partner start aligned, CINE suppresses or hands off control as designed, and End restores gameplay control

### Requirement: Phase 3 engineering gate
Phase 3 engineering validation SHALL demonstrate Sync Group Fire (per-member instructions, hard-cut default), CINE Enter/End with in-CINE marking, and Edit preview of Fire/marks without Behaviour Tick.

#### Scenario: Harness gate
- **WHEN** the Phase 3 engineering harness is exercised
- **THEN** multi-Player Fire alignment and CINE Enter/End are observable under automation and/or Edit preview

### Requirement: Parallel with Phase 2 weighted gate
Phase 3 engine implementation MAY proceed while Phase 2 Chocomel weighted Play acceptance remains open. Phase 2 Done criteria SHALL remain in force and SHALL NOT be silently replaced by Phase 3 SYNC/CINE acceptance alone.

#### Scenario: Both bars tracked
- **WHEN** Phase 3 work starts before Phase 2 Chocomel weighted acceptance closes
- **THEN** both Phase 2 weighted and Phase 3 mini SYNC/CINE Done bars remain separately tracked
