## ADDED Requirements

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

### Requirement: Pause skips Behaviour Tick
While paused, the Player SHALL skip gameplay Behaviour Tick (and equivalent gameplay simulation time) but SHALL keep the process and window alive so the world remains viewable. Resume SHALL continue Tick from the paused world state.

#### Scenario: Pause then Resume
- **WHEN** the Player receives pause then later resume
- **THEN** Tick does not advance during pause and resumes afterward without requiring a process restart

### Requirement: Window close ends Play Process
Closing the Player OS window SHALL exit the Player process.

#### Scenario: Close window
- **WHEN** the author closes the Player window
- **THEN** the process exits and the editor session becomes Stopped
