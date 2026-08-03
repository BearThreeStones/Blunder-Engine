## ADDED Requirements

### Requirement: Sync Group runtime coordination
The engine SHALL provide a **Sync Group** as a runtime set of AnimationPlayer members that scripts can create, join, leave, fire, and destroy. Sync Group SHALL NOT require a serialized scene-component member graph as the supported product path. Sync Group SHALL NOT introduce a shared continuous playback head across members or an AnimationTree.

#### Scenario: Runtime create and fire
- **WHEN** a script creates a Sync Group, adds two AnimationPlayers, and Fires with per-member clip instructions
- **THEN** both players start (or seek) at the same logical moment using those instructions

#### Scenario: No required scene bake
- **WHEN** an author forms a temporary character+prop sync for one interaction
- **THEN** the group can be created and released at runtime without a pre-authored scene Sync Group asset as the only path

### Requirement: Per-member Fire instructions
Sync Group Fire SHALL accept per-member instructions of the form `(AnimationPlayer, clipName[, seek])`. Heterogeneous clip logical names across members SHALL be supported. Same logical name across all members MAY be offered as convenience sugar but SHALL NOT be required.

#### Scenario: Heterogeneous clip names
- **WHEN** Fire assigns `CINE-character-attach` to player A and `CINE-prop-attach` to player B
- **THEN** both clips begin at the same logical moment without requiring identical names

### Requirement: Default Fire is hard cut
Sync Group Fire SHALL default to hard-cut clip starts (`Play` with fade duration zero or equivalent snap). Phase 2 Crossfade SHALL NOT be the default Sync Group Fire path.

#### Scenario: Hard-cut alignment
- **WHEN** members were previously blending or playing other clips and Fire runs with default semantics
- **THEN** each member snaps to the instructed clip without a Sync-Group-driven Crossfade ramp

### Requirement: Co-located Skeleton drive unchanged
Phase 3 Sync Group SHALL coordinate multiple AnimationPlayers. Each AnimationPlayer SHALL continue to drive only the Skeleton on the same Object. Cross-Object Skeleton drive remains out of scope.

#### Scenario: Prop and character each own bones
- **WHEN** a character Object and a prop Object are Sync Group members
- **THEN** each player deforms only its co-located Skeleton

## ADDED Requirements (CINE)

### Requirement: CINE segment enter and explicit end
The engine SHALL support a **CINE** segment contract with Enter and an **explicit End** API. Segment end is authoritative when scripts call End. Member AnimationPlayer `finished` notifications MAY assist authors but SHALL NOT alone end the segment.

#### Scenario: Explicit end
- **WHEN** a CINE segment is active and a script calls End
- **THEN** in-CINE marking clears and the segment is no longer active even if some clips are still playing

#### Scenario: Finished does not auto-end
- **WHEN** a lead clip finishes while the segment is active and no End was called
- **THEN** the segment remains active until End (unless a future explicit opt-in auto-end is added outside this requirement)

### Requirement: in-CINE mark and optional input suppression
While a CINE segment is active, the engine SHALL expose an in-CINE mark and MAY suppress gameplay input according to a documented channel set. Pose alignment and gameplay state-machine transitions remain C# Behaviour responsibilities.

#### Scenario: Input suppressed in CINE
- **WHEN** Enter CINE enables input suppression
- **THEN** configured gameplay actions do not drive movement until End restores them

### Requirement: Edit Mode SYNC/CINE preview without Behaviour
Edit Mode SHALL allow Sync Group Fire and CINE Enter/End preview so authors can observe in-CINE marking and multi-Skeleton playback without starting DotNetHost / Behaviour Tick. Edit Mode SHALL NOT auto-snap Object TRS or run gameplay state machines as the Phase 3 preview path.

#### Scenario: Edit fire visible
- **WHEN** an author Fires a Sync Group in Edit Mode for two skinned Objects
- **THEN** both Skeletons play the instructed clips and in-CINE can be toggled without Behaviour Tick

#### Scenario: No auto TRS handoff in Edit
- **WHEN** Enter/End CINE runs in Edit Mode
- **THEN** Object world transforms are not required to auto-snap for preview success
