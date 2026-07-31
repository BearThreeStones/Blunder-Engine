## ADDED Requirements

### Requirement: Unified two-slot blend model
AnimationPlayer SHALL sample at most two AnimationClips (slot0 and slot1) and combine poses with a single blendWeight in [0, 1], where 0 is entirely slot0 and 1 is entirely slot1. Pose combine SHALL use local translation/scale linear interpolation and rotation spherical linear interpolation. Phase 2 SHALL NOT require additive blend layers or N-way blend graphs.

#### Scenario: Weighted dual-track
- **WHEN** slot0 plays idle, slot1 plays walk, and blendWeight is 0.5
- **THEN** the Skeleton pose is the local TRS blend of both sampled poses at that weight

#### Scenario: Explicit slot APIs
- **WHEN** a script assigns clip names to slots and sets blendWeight
- **THEN** subsequent samples use those slots and weight without requiring an AnimationTree

### Requirement: Crossfade is a weight ramp on the two slots
Soft clip switches SHALL be expressed on the same two-slot model: replacing a slot’s clip and advancing blendWeight over a fade duration. `Play(name, fade)` MAY be convenience sugar over that model. Phase 2 SHALL NOT introduce a third simultaneous sample path for Crossfade.

#### Scenario: Play with fade
- **WHEN** Play is called with fade duration greater than zero while another clip is active
- **THEN** the player Crossfades toward the requested clip using the two-slot weight ramp

### Requirement: Hard cut remains fade zero
Phase 1 hard-cut behaviour SHALL remain available: Play with fade duration zero (or equivalent default) SHALL snap to the new clip without a Crossfade ramp.

#### Scenario: Hard cut via fade zero
- **WHEN** Play is called with fade duration zero while another clip is playing
- **THEN** the new clip replaces the previous immediately (no Phase 2 Crossfade)

### Requirement: Global TimeScale
AnimationPlayer SHALL expose a single TimeScale multiplier that scales advance of all active slots together. Phase 2 SHALL NOT require per-track TimeScale.

#### Scenario: Global rate
- **WHEN** TimeScale is set to 0.5 while two slots are sampling
- **THEN** both slots advance at half rate under the same multiplier

### Requirement: Dominant-slot playback position under blend
Under two-slot blend, the playback position exposed for Animation step detection SHALL be the **dominant slot** (higher blendWeight; while Crossfading, the fade target). The engine SHALL NOT require a blended-time step clock or a builtin AnimationStep event.

#### Scenario: Step clock follows dominant
- **WHEN** blendWeight favors slot1 (walk) and PoseApplied fires
- **THEN** scripts can read a playback position corresponding to the dominant (walk) slot for frametime-modulo logic

### Requirement: Scene defaults for Phase 2 playback
Scene serialization SHALL persist the name→GUID map and MAY persist Phase 2 defaults: TimeScale and optional default slot clip names with initial blendWeight. Scenes SHALL NOT be required to persist full live playback-head or in-flight Crossfade snapshots.

#### Scenario: Defaults round-trip
- **WHEN** an author saves default TimeScale and initial two-slot setup on an AnimationPlayer
- **THEN** reloading the scene restores those defaults

### Requirement: Edit Mode scrub for blend and TimeScale
Edit Mode SHALL allow authorship controls to adjust TimeScale, fade, and two-slot weights on AnimationPlayer without starting the Project .NET host or running Behaviour Tick. Stepped facing validation SHALL remain a Play Mode concern.

#### Scenario: Edit scrub without Behaviour
- **WHEN** the user adjusts blendWeight or TimeScale in Edit preview
- **THEN** the Skeleton updates from AnimationPlayer sampling and Behaviours do not Tick for that preview path

## MODIFIED Requirements

### Requirement: AnimationPlayer plays clips by logical name
AnimationPlayer SHALL expose Play, Stop, and Loop over logical clip names. It SHALL persist a name→AnimationClip Asset Reference (GUID) map. Play SHALL resolve the name through that map. Phase 1 transitions between clips SHALL be hard cuts when fade duration is zero. Phase 2 SHALL additionally allow Crossfade when fade duration is greater than zero, using the unified two-slot model.

#### Scenario: Play by name
- **WHEN** a script calls Play with a mapped clip name (e.g. `LOOP-chocomel-walk`)
- **THEN** the player begins sampling that AnimationClip onto the Skeleton

#### Scenario: Unknown name
- **WHEN** Play is called with a name absent from the map
- **THEN** playback does not start that clip (no crash; error or no-op per engine logging policy)

#### Scenario: Hard cut
- **WHEN** Play is called with fade duration zero while another clip is playing
- **THEN** the new clip replaces the previous without Crossfade

#### Scenario: Soft cut
- **WHEN** Play is called with fade duration greater than zero while another clip is playing
- **THEN** the player Crossfades toward the new clip on the two-slot model
