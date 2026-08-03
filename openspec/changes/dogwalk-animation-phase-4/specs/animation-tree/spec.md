## ADDED Requirements

### Requirement: AnimationTree ClassDB playback graph
The engine SHALL provide an **AnimationTree** ClassDB type co-located on the same Object as **AnimationPlayer** and **Skeleton**. AnimationTree SHALL resolve clips via the AnimationPlayer logical name→GUID map. Phase 4 SHALL support a lean node set: **StateMachine**, **BlendSpace1D**, **OneShot**, and **Add2**. AnimationTree SHALL NOT require AnimationLibrary, BlendSpace2D, or full Godot AnimationTree parity.

#### Scenario: Co-located graph resolves clips by name
- **WHEN** an AnimationTree on an Object samples a node that references clip logical name `LOOP-chocomel-walk`
- **THEN** the clip is resolved through that Object's AnimationPlayer name→GUID map and contributes to the Skeleton pose

### Requirement: Active tree exclusive Skeleton sampling
When AnimationTree is **active**, it SHALL exclusively own Skeleton sampling for that Object. AnimationPlayer Play / two-slot / Crossfade SHALL NOT write bones while the tree is active. When the tree is inactive, Phase 1–3 AnimationPlayer sampling SHALL resume.

#### Scenario: Active tree blocks player bone writes
- **WHEN** AnimationTree is active and a script calls AnimationPlayer Play or sets two-slot weights
- **THEN** subsequent Skeleton poses come from the tree sample path, not from Player two-slot output

#### Scenario: Inactive falls back to player
- **WHEN** AnimationTree is set inactive after being active
- **THEN** AnimationPlayer Play / two-slot sampling writes the Skeleton again under Phase 1–3 rules

### Requirement: Base then Add2 sample stack
AnimationTree SHALL sample a **base** pose from the StateMachine (a state MAY be a BlendSpace1D or a single clip; OneShot MAY temporarily insert/replace over that base) and THEN apply optional **Add2** additive layers. Additive deltas SHALL be relative to **bind/rest**. Phase 4 SHALL NOT treat Add2 as a Phase 2 lerp dual-track slot.

#### Scenario: Additive turn on locomotion base
- **WHEN** the base graph outputs a locomotion pose and Add2 has a non-zero weight with an additive turn clip
- **THEN** the final Skeleton pose is the base pose combined with bind/rest-relative additive deltas

### Requirement: BlendSpace1D
AnimationTree SHALL provide **BlendSpace1D** nodes that blend among discrete authored clip points along one scalar parameter. Neighbor blends SHALL use local TRS lerp / rotation slerp (same family as Phase 2). Phase 4 SHALL NOT require BlendSpace2D.

#### Scenario: Speed-like blend
- **WHEN** a BlendSpace1D has walk and trot points and the scalar sits between them
- **THEN** the base pose is the local TRS blend of the neighboring sampled clips

### Requirement: StateMachine travel and start
AnimationTree SHALL provide a **StateMachine** node that selects among named animation states via script **Travel** / **Start**. Each state's playback SHALL be a BlendSpace1D or a single clip. The animation StateMachine SHALL remain distinct from gameplay / character state machines in C# Behaviours. Nested Godot-complete transition graphs SHALL NOT be required for Phase 4 Done.

#### Scenario: Travel to locomotion state
- **WHEN** a script calls Travel to a named locomotion state whose playback is a BlendSpace1D
- **THEN** subsequent base samples come from that state's BlendSpace1D

### Requirement: OneShot insert and return
AnimationTree SHALL provide **OneShot** that inserts a clip (or short sub-pose) over the base graph and then returns to the underlying StateMachine/BlendSpace base.

#### Scenario: Trip-like interrupt
- **WHEN** RequestOneShot plays a trip clip while locomotion base is active
- **THEN** the tree samples the OneShot while active and returns to the locomotion base afterward

### Requirement: Narrow named script API
Scripts SHALL drive AnimationTree through a narrow named API (`Travel` / `Start`, per-node BlendSpace1D scalar by **node logical name**, `RequestOneShot`, Add2 weight/clip setters). Godot-style `parameters/…` path strings SHALL NOT be the Phase 4 primary script surface. A single anonymous global blend float with no node name SHALL NOT be required when multiple BlendSpace1D nodes exist.

#### Scenario: Per-node blend scalar
- **WHEN** a script sets the BlendSpace1D scalar on node logical name `Locomotion`
- **THEN** only that node's parameter updates and sampling reflects the new scalar

### Requirement: Scene-embedded topology
AnimationTree graph topology (states, BlendSpace points by clip logical name + scalar, OneShot/Add2 slots) SHALL persist as scene-embedded data on the AnimationTree. Phase 4 Done SHALL NOT require a visual node graph editor or a standalone AnimationTree Asset.

#### Scenario: Topology round-trip
- **WHEN** an author saves an Object with an embedded AnimationTree BlendSpace1D + StateMachine configuration
- **THEN** reloading the scene restores that topology without a separate Tree Asset

### Requirement: PoseApplied and base dominant-clip step clock
With AnimationTree active, the engine SHALL still emit PoseApplied after sampling. Playback position exposed for Animation step detection SHALL follow the **base dominant clip** clock (BlendSpace1D: highest-weight neighbor; StateMachine: current state's clip/dominant point; while OneShot plays: the OneShot clip). **Add2 SHALL NOT** supply the step clock. Advance SHALL remain under AnimationPlayer **TimeScale** as a single global multiplier. The engine SHALL NOT require a blended multi-clip step clock or a builtin AnimationStep event.

#### Scenario: Step follows dominant blend neighbor
- **WHEN** BlendSpace1D weight favors the trot point and PoseApplied fires
- **THEN** scripts can read a playback position corresponding to the trot (dominant) clip for frametime-modulo logic

### Requirement: Edit Mode AnimationTree scrub
Edit Mode SHALL allow activating AnimationTree and scrubbing named drives (BlendSpace scalars, Travel/Start, OneShot, Add2 weights, TimeScale) without starting DotNetHost / Behaviour Tick. Stepped facing validation SHALL remain a Play Mode concern. Edit Mode SHALL NOT require a visual graph editor for Phase 4 Done.

#### Scenario: Edit scrub without Behaviour
- **WHEN** an author activates the tree and adjusts a BlendSpace1D scalar in Edit preview
- **THEN** the Skeleton updates from tree sampling and Behaviours do not Tick for that preview path
