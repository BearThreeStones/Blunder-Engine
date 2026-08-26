## ADDED Requirements

### Requirement: Player forwards Console Messages on the control channel
The Player SHALL send Console Messages (severity, text, optional Console stack, time) to the editor on the existing Play control channel. It SHALL NOT use Player stdout capture or a second socket as the product feed. Session commands (pause, resume, stop) SHALL remain on that same connection.

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
