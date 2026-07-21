## ADDED Requirements

### Requirement: Local IPC control channel
The editor and the single Play Process SHALL communicate over a local IPC channel (not internet-facing) used for session commands. The first slice SHALL support at least pause, resume, and stop. Process exit SHALL also end Play Mode on the editor side.

#### Scenario: Pause command
- **WHEN** the editor sends pause over the control channel to a Playing Player
- **THEN** the Player enters paused Tick behavior

#### Scenario: Stop command
- **WHEN** the editor sends stop
- **THEN** the Player begins shutdown and exits

#### Scenario: Endpoint passed at spawn
- **WHEN** the editor spawns the Player for a session
- **THEN** the Player receives the IPC endpoint needed to connect to the editor-held local listen socket and receive commands for that session

### Requirement: Ready before Pause
The editor SHALL NOT treat Pause as successful until the Player has signaled that the control channel is ready (or an equivalent handshake), unless the session has already failed/exited.

#### Scenario: Pause disabled until ready
- **WHEN** the Player has been spawned but has not yet signaled ready
- **THEN** the editor does not assume pause is in effect
