# play-player Specification

## Purpose
Dedicated `engine_player` process for Play Mode: load Project + entry scene, host Scripts, mount Behaviours, Pause skips Tick, window close ends the session.
## Requirements
### Requirement: Dedicated Player executable
The product SHALL ship an `engine_player` executable (thin entry over the shared engine runtime) that runs Play Mode without the editor authorship shell.

#### Scenario: Sibling binary
- **WHEN** the editor build/package is produced
- **THEN** `engine_player` is available beside `engine_editor` for spawn

### Requirement: Player loads Project and entry scene
On start, the Player SHALL accept a Project root and a Play entry scene reference (saved scene asset). It SHALL load that scene into its own world (not the editor’s authorship SceneInstance).

#### Scenario: Load saved scene
- **WHEN** the Player is started with a valid project root and scene asset path/GUID
- **THEN** the scene loads successfully in the Player process

### Requirement: Player runs DotNetHost for the session
The Player SHALL start the .NET script host for the Play session, register the process NativeAbi, load the Project game assembly when present, and mount scene Behaviours when the host and assembly are ready. Host or Scripts failure SHALL be non-fatal to process start when the scene can still load (null peers allowed).

#### Scenario: Mount in Player
- **WHEN** the entry scene declares a Behaviour type present in the game assembly and the host started
- **THEN** managed peers are attached and Tick can reach them while not paused

### Requirement: Headless Player has no OS window
A Headless Player SHALL NOT create an OS window. Play frame SHALL be a still of the Play-rule camera color target (CPU readback), not an HWND scrape. Ending a Headless Player SHALL be Stop on the Play control channel or process exit.

#### Scenario: Play frame without a window
- **WHEN** a Headless Player is running and Play frame is requested
- **THEN** a 16:9 Play frame of the Play Process world is sent on the Play control channel
- **AND** no Player OS window exists

### Requirement: Pause skips Behaviour Tick
While paused, the Player SHALL skip gameplay Behaviour Tick (and equivalent gameplay simulation time) but SHALL keep the process alive (and, when windowed, keep the window alive so the world remains viewable). Resume SHALL continue Tick from the paused world state.

#### Scenario: Pause then Resume
- **WHEN** the Player receives pause then later resume
- **THEN** Tick does not advance during pause and resumes afterward without requiring a process restart

### Requirement: Window close ends Play Process
On a windowed Player, closing the Player OS window SHALL exit the Player process. This SHALL NOT apply to a Headless Player (there is no OS window).

#### Scenario: Close window
- **WHEN** the author closes the Player window
- **THEN** the process exits and the editor session becomes Stopped

#### Scenario: Headless has no window close
- **WHEN** a Headless Player is running
- **THEN** session end is Stop on the Play control channel or process exit, not window close

### Requirement: Player is not an Editor Overlay surface
The Player SHALL NOT draw or hit-test Editor Overlays (authorship viewport chrome including ground grid, Transform gizmo, Navigate gizmo, and related overlays). The editor viewport SHALL keep Editor Overlays while a Play Session is running.

#### Scenario: Play shows gameplay without authorship chrome
- **WHEN** Play Mode runs in the Player process
- **THEN** the Player game view has no Editor Overlay authorship chrome

### Requirement: Player authorship isolation
The Player host SHALL NOT accept Editor Camera interaction, viewport pick, Transform/Navigate gizmo input, or other authorship shortcuts. A windowed Player SHALL accept Gameplay Input when focused and not paused, plus system window chrome (e.g. close). A Headless Player has no window chrome. Editor Overlay draw/interaction remains disabled in Player (existing).

#### Scenario: No Editor Camera orbit in Player
- **WHEN** the user uses RMB/MMB/orbit shortcuts in the Player window
- **THEN** the Player view SHALL NOT orbit via Editor Camera

#### Scenario: Player view from scene Camera
- **WHEN** Play is running and the entry scene has a resolvable Camera Component
- **THEN** each Player frame SHALL use that resolved view/projection, not Editor Camera matrices

### Requirement: Player forwards Console Messages on the control channel
The Player SHALL send Console Messages (severity, text, optional Console stack, time) to the editor on the existing Play control channel. It SHALL NOT use Player stdout capture or a second socket as the product feed. Session commands (pause, resume, stop, Play step, Play frame, Play Reload, Play authorship patch) and Player → editor Play pose preview SHALL remain on that same connection.

#### Scenario: Debug.Log is forwarded
- **WHEN** a Behaviour in the Player calls `Debug.Log("hello")` and the control channel is ready
- **THEN** the Player writes a Log record on that channel that the editor can ingest

#### Scenario: Engine warn in Player is forwarded
- **WHEN** the Player engine logs at warn level
- **THEN** a Warning record is sent on the control channel

### Requirement: Player does not AllocConsole
The Player SHALL NOT allocate a new OS console window as the product path.

#### Scenario: Spawned Player has no new system console
- **WHEN** the editor spawns `engine_player` as a GUI-subsystem process
- **THEN** the Player does not create a new OS console window

### Requirement: Player survives Lifecycle exceptions
A Lifecycle exception in the Player SHALL NOT end the Play Process. Tick/Ready/OnMessage/PoseApplied remaining invocations in that frame SHALL follow debug-api rules.

#### Scenario: Thrown Tick does not exit
- **WHEN** a Behaviour Tick throws in the Player
- **THEN** the process is still running afterward
- **AND** an Error record is forwarded on the control channel

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

