## ADDED Requirements

### Requirement: Tree parameters on AnimationTree
AnimationTree SHALL support a small set of independent bool/float **tree parameters** (named) that scripts and Inspector can set/get, usable as StateMachine transition condition inputs alongside existing named drives.

#### Scenario: Set and read float param
- **WHEN** a script sets tree parameter `speedGate` to `0.75`
- **THEN** subsequent transition evaluation can read `0.75` for that name

### Requirement: Transition evaluation on advance
When AnimationTree is active, the StateMachine SHALL evaluate outgoing transition edges on the animation advance path and MAY auto-Travel per StateMachine transition rules before sampling the resulting current state's playback for that advance.

#### Scenario: Active tree evaluates edges
- **WHEN** AnimationTree is active and an outgoing edge condition becomes true
- **THEN** the tree's current state updates per transition rules without requiring the author to call Travel for that edge
