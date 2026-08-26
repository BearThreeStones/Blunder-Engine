## ADDED Requirements

### Requirement: Headless Player has no OS window
A Headless Player SHALL NOT create an OS window. Play frame SHALL be a still of the Play-rule camera color target (CPU readback), not an HWND scrape. Ending a Headless Player SHALL be Stop on the Play control channel or process exit.

#### Scenario: Play frame without a window
- **WHEN** a Headless Player is running and Play frame is requested
- **THEN** a 16:9 Play frame of the Play Process world is sent on the Play control channel
- **AND** no Player OS window exists

## MODIFIED Requirements

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
