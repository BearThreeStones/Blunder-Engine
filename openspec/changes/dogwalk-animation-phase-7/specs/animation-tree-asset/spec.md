## ADDED Requirements

### Requirement: Asset stores transitions parameters and layout
An **AnimationTree Asset** body SHALL be able to persist StateMachine **transition edges** (targets, conditions, priorities), **tree parameters** declarations/defaults as applicable, and **Canvas layout** together with existing topology (states, BlendSpaces, OneShot/Add2 slots).

#### Scenario: Asset round-trip includes edges and layout
- **WHEN** an Asset is saved with two transition edges, one tree param, and canvas node positions
- **THEN** reloading the Asset restores edges, param metadata, and layout

### Requirement: Canvas is optional relative to Phase 5 D
Authors SHALL still be able to create and edit AnimationTree Assets via Inspector without opening the Canvas. Phase 5 D Inspector authorship remains valid; Canvas is additive for Phase 7.

#### Scenario: Inspector-only Asset still valid
- **WHEN** an author never opens the Canvas and edits topology only in Inspector
- **THEN** the Asset remains loadable and usable at runtime
