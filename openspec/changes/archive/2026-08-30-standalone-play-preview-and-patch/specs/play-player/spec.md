## ADDED Requirements

### Requirement: Player reinstantiates the captured Play entry on Reload
When the Player receives Play Reload, it SHALL unload the current gameplay world and instantiate the session’s captured On-disk Play entry scene into a new world in the same process. It SHALL read that asset from disk (after editor Save rules), not a cached stale document, and SHALL NOT dump the editor Live SceneInstance. On instantiate failure it SHALL keep the current world and remain running.

#### Scenario: Reload replaces the world in-process
- **WHEN** the Player receives Play Reload and the On-disk Play entry instantiates
- **THEN** the previous gameplay world is gone
- **AND** the new world is the instantiated saved Play entry
- **AND** the process is still the same Play Process

#### Scenario: Failed instantiate keeps the world
- **WHEN** the Player receives Play Reload and instantiate fails
- **THEN** the previous gameplay world remains
- **AND** the process is still running

### Requirement: Player remounts Behaviours from the loaded assembly
On Play Reload, the Player SHALL remount scene Behaviours from the Scripts assembly already loaded in that process. It SHALL NOT run `dotnet build` and SHALL NOT load a replacement assembly as part of Reload. Ready SHALL run on that remount even while paused.

#### Scenario: Ready on remount while paused
- **WHEN** the Player is paused and Reload instantiates successfully
- **THEN** Ready has run on remounted Behaviours
- **AND** Tick stays skipped until resume or Play step

### Requirement: Player applies Play authorship patches
When the Player receives a v1 Play authorship patch, it SHALL write the authored data onto the Play Process entity at the given Authorship Address. If that address is missing, it SHALL skip the write and SHALL emit a Warning-grade Issue with code `play.patch_unknown_address` on the Play control channel. It SHALL apply patches while Playing or Paused.

#### Scenario: Named entity is written
- **WHEN** the Player receives a Local Transform patch for Authorship Address `Hero` and that entity exists
- **THEN** that entity’s Local Transform in the Play Process world matches the patch

#### Scenario: Unknown address is a Warning
- **WHEN** the Player receives a v1 patch for an Authorship Address that does not exist
- **THEN** no entity is written
- **AND** a Warning record with code `play.patch_unknown_address` is sent on the control channel

### Requirement: Player emits Play pose preview
While a Play session is connected, the Player SHALL send Play pose preview records on the Play control channel for Play-entry entities that have an Authorship Address. It SHALL NOT send preview for runtime-spawned entities with no Authorship Address. It SHALL NOT treat those records as Play frames.

#### Scenario: Poses for named entities
- **WHEN** the Player world has a Play-entry entity named `Hero` and the control channel is ready
- **THEN** the Player sends pose records for Authorship Address `Hero` on that channel

## MODIFIED Requirements

### Requirement: Player forwards Console Messages on the control channel
The Player SHALL send Console Messages (severity, text, optional Console stack, time) to the editor on the existing Play control channel. It SHALL NOT use Player stdout capture or a second socket as the product feed. Session commands (pause, resume, stop, Play step, Play frame, Play Reload, Play authorship patch) and Player → editor Play pose preview SHALL remain on that same connection.

#### Scenario: Debug.Log is forwarded
- **WHEN** a Behaviour in the Player calls `Debug.Log("hello")` and the control channel is ready
- **THEN** the Player writes a Log record on that channel that the editor can ingest

#### Scenario: Engine warn in Player is forwarded
- **WHEN** the Player engine logs at warn level
- **THEN** a Warning record is sent on the control channel
