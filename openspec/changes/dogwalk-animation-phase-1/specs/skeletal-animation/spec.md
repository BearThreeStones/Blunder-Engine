## ADDED Requirements

### Requirement: Skeleton co-located with AnimationPlayer
A skinned character Object SHALL host both a Skeleton and an AnimationPlayer. Phase 1 AnimationPlayer SHALL drive only the Skeleton on the same Object. Cross-Object skeleton references are out of scope for Phase 1.

#### Scenario: Same Object hosts both
- **WHEN** an author sets up a Phase 1 animated character
- **THEN** Skeleton and AnimationPlayer are present on one Object and the player samples onto that Skeleton

#### Scenario: Cross-Object drive rejected or unsupported
- **WHEN** content attempts to bind AnimationPlayer to a Skeleton on a different Object in Phase 1
- **THEN** the engine does not treat that as a supported Phase 1 configuration (no silent remote drive)

### Requirement: AnimationPlayer plays clips by logical name
AnimationPlayer SHALL expose Play, Stop, and Loop over logical clip names. It SHALL persist a name→AnimationClip Asset Reference (GUID) map. Play SHALL resolve the name through that map. Phase 1 transitions between clips SHALL be hard cuts (no blend graph).

#### Scenario: Play by name
- **WHEN** a script calls Play with a mapped clip name (e.g. `LOOP-chocomel-walk`)
- **THEN** the player begins sampling that AnimationClip onto the Skeleton

#### Scenario: Unknown name
- **WHEN** Play is called with a name absent from the map
- **THEN** playback does not start that clip (no crash; error or no-op per engine logging policy)

#### Scenario: Hard cut
- **WHEN** Play is called while another clip is playing
- **THEN** the new clip replaces the previous without Phase 1 crossfade/blend

### Requirement: PoseApplied after sample
Each frame, after AnimationPlayer finishes sampling the current clip into Skeleton pose, the engine SHALL notify listeners (PoseApplied or equivalent). Scripts SHALL be able to read playback position/length for Animation step detection. The engine SHALL NOT emit a builtin on-2s AnimationStep event as the Phase 1 product API.

#### Scenario: Callback after pose write
- **WHEN** a clip is sampling during a frame
- **THEN** PoseApplied runs after Skeleton pose for that sample is applied

#### Scenario: Playback time readable
- **WHEN** a Behaviour handles PoseApplied or polls the player after sample
- **THEN** current play position and clip length are available for frametime-modulo step logic

### Requirement: Per-frame order with Behaviour Tick
In Play Mode, the engine SHALL process Behaviour Tick before AnimationPlayer sampling for that frame’s gameplay→animation contract, then PoseApplied after sampling. Behaviours SHALL NOT treat Tick as the supported place to read the final Skeleton pose for that frame’s sample.

#### Scenario: Play from Tick takes effect same frame sample
- **WHEN** a Behaviour in Tick calls Play or updates Object motion
- **THEN** the subsequent sample in the same frame uses that Play request

#### Scenario: Stepped visuals use PoseApplied
- **WHEN** a Behaviour updates stepped facing from animation timing
- **THEN** that update runs from PoseApplied (or equivalent post-sample hook), not as the documented Tick pattern

### Requirement: Constant and Linear sampling
AnimationClip sampling SHALL support Constant and Linear interpolation modes as recorded on tracks. Phase 1 SHALL NOT require Cubic or Bezier sampling.

#### Scenario: Constant holds
- **WHEN** a track is Constant/Stepped and time lies between keys
- **THEN** the sampled value equals the preceding key

#### Scenario: Linear interpolates
- **WHEN** a track is Linear and time lies between keys
- **THEN** the sampled value interpolates between surrounding keys

### Requirement: Edit Mode AnimationPlayer preview
Edit Mode SHALL allow preview Play/Pause/Stop/Loop of AnimationPlayer clips on the authorship viewport Skeleton without starting the Project .NET host or running Behaviour Tick for that preview.

#### Scenario: Preview without scripts
- **WHEN** the user previews a clip in Edit Mode
- **THEN** the Skeleton animates from AnimationPlayer sampling and Behaviours do not Tick for that preview path

#### Scenario: Play Mode still owns gameplay feel
- **WHEN** Stepped facing / ValueSlicer behaviour is validated
- **THEN** acceptance is under Play Mode, not Edit preview alone
