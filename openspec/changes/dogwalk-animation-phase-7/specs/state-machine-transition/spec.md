## ADDED Requirements

### Requirement: Conditional StateMachine transitions
The AnimationTree StateMachine SHALL support authored **transition edges** that can **automatically Travel** to a target state when a condition holds. Edges SHALL drive runtime behavior (not decoration-only).

#### Scenario: Condition true travels
- **WHEN** the current state has an outgoing edge whose condition is true and it wins priority
- **THEN** the StateMachine hard-cuts to the edge's target state on the animation advance path

### Requirement: Single predicate per edge
Each transition edge's condition SHALL be a **single predicate**: `param op value` with ops among `==`, `!=`, `<`, `<=`, `>`, `>=`, or a bool param truth check. AND/OR bundles or free expression languages on one edge SHALL NOT be required for Phase 7 Done. Authors MAY compose behavior via multiple edges and priority.

#### Scenario: Scalar threshold
- **WHEN** an edge condition is `Locomotion >= 0.5` (or equivalent drive/param binding) and the value crosses the threshold
- **THEN** the edge may fire subject to priority rules

### Requirement: Hybrid condition inputs
Transition predicates SHALL be able to read **existing named tree drives** (BlendSpace1D scalar, BlendSpace2D x/y, Add2 weight, as applicable) **and** independent bool/float **tree parameters** set by script or Inspector.

#### Scenario: Independent bool param
- **WHEN** a script sets tree parameter `wantTrip` true and an edge from Locomotion to TripState checks that bool
- **THEN** the StateMachine auto-Travels to TripState (subject to priority)

### Requirement: Priority on multi-true edges
When multiple outgoing edges from the current state evaluate true in one evaluation, the engine SHALL select the edge with the highest author **priority**. Ties SHALL break by a stable order (e.g. declaration order).

#### Scenario: Higher priority wins
- **WHEN** two outgoing edges are both true and priorities differ
- **THEN** the higher-priority edge's target becomes the new state

### Requirement: Hard-cut switch
Auto transition SHALL apply a **hard cut** into the target state (same family as script Travel). Per-edge fade/Crossfade SHALL NOT be required for Phase 7 Done.

#### Scenario: No fade required
- **WHEN** an auto transition fires
- **THEN** the next sample uses the target state's playback without requiring an edge-duration blend

### Requirement: Travel and Start coexist
Script **Travel** / **Start** SHALL remain first-class and MAY force a state change even when no transition edge applies or fires.

#### Scenario: Forced Travel
- **WHEN** no outgoing edge is true and a script calls Travel to a named state
- **THEN** the StateMachine enters that state
