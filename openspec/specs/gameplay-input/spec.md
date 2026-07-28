# gameplay-input Specification

## Purpose
Player-authoritative Gameplay Actions (Move axis + Jump press-edge) sampled each simulation frame and exposed to Behaviours via a static `Blunder.Api` `Input` façade over NativeAbi.

## Requirements

### Requirement: Player is the authoritative Gameplay Input source
Gameplay Input SHALL be authoritative only while the process is running as the Player host in Play Mode. Outside Player host mode, Action reads SHALL return idle (Move zero, Jump false).

#### Scenario: Non-Player host reads idle
- **WHEN** a Behaviour or C-ABI caller reads Gameplay Actions in Editor host mode
- **THEN** Move is (0, 0) and Jump pressed is false

#### Scenario: Player host can produce non-idle Actions
- **WHEN** the Player host is focused, not Play-paused, and default Move keys are held
- **THEN** Move reflects the sampled axis (within the default binding rules)

### Requirement: Player input surface
In `EngineHostMode::Player`, gameplay-facing input sampling SHALL remain Gameplay Input (focused, not paused). Authorship input paths (Editor Camera, pick, gizmos) SHALL be excluded from the Player host regardless of focus.

#### Scenario: Focused Player gets Gameplay Input only
- **WHEN** the Player window is focused and Play is not paused
- **THEN** Gameplay Input MAY be sampled AND authorship input SHALL remain disabled

### Requirement: Starter Actions are Move axis and Jump press-edge
The first Gameplay Input slice SHALL expose a 2D **Move** Action and a **Jump** Action. Jump SHALL be a pressed-edge for the current simulation frame. Move SHALL use built-in defaults WASD with opposing keys canceling and diagonal vectors normalized so length is at most 1. Move `(x, y)` SHALL map to world **+X (right)** and **+Y (forward)** on the horizontal plane.

#### Scenario: Diagonal Move is normalized
- **WHEN** W and D are held together with no opposing keys
- **THEN** Move length is approximately 1 (not √2)

#### Scenario: Jump is press-edge not held level
- **WHEN** Space is held across multiple simulation frames after the initial press frame
- **THEN** Jump pressed is true only on the frame of the down transition, not on subsequent held frames

#### Scenario: Opposing Move keys cancel
- **WHEN** A and D are both held
- **THEN** Move.x is 0

### Requirement: Jump edge is shared for the simulation frame
Every Behaviour that polls Jump during the same simulation Tick SHALL observe the same Jump pressed value. Polling SHALL NOT consume or clear the edge for later readers in that frame.

#### Scenario: Two Behaviours see the same Jump
- **WHEN** Jump pressed is true for the current frame and Behaviour A then Behaviour B both poll Jump in the same Tick
- **THEN** both polls return true

### Requirement: Pause discards Action edges
While Play Pause is active, Gameplay Input SHALL NOT accumulate Jump edges for a later Tick, and Move SHALL read as idle. A Jump that occurs only during Pause SHALL NOT appear on the first Resume Tick.

#### Scenario: Jump only during Pause is discarded
- **WHEN** Play is paused, Space is pressed and released during Pause, then Play resumes
- **THEN** the first Resume Tick reports Jump pressed false

#### Scenario: Move idle while paused
- **WHEN** Play is paused and WASD are held
- **THEN** Move reads as (0, 0)

### Requirement: Unfocused Player yields idle Actions
When the Player window does not have OS focus, Gameplay Input SHALL be idle (Move zero, Jump false).

#### Scenario: Unfocused while keys held
- **WHEN** the Player window is unfocused and WASD or Space would otherwise produce Actions
- **THEN** Move is (0, 0) and Jump pressed is false

### Requirement: Managed Input façade polls Actions
`Blunder.Api` SHALL expose a static `Input` type that Behaviours use to poll Move and Jump through the registered NativeAbi table. Gameplay Input SHALL NOT be exposed as Object/ClassDB properties in this slice.

#### Scenario: Behaviour polls Move via Input
- **WHEN** NativeAbi is registered and a Behaviour calls `Input.GetMove` during Player Tick
- **THEN** the returned components match the native Action snapshot for that frame

#### Scenario: Call before NativeAbi register fails clearly
- **WHEN** managed code calls `Input` before NativeAbi registration
- **THEN** the call fails with a clear error and MUST NOT silently DllImport a second engine image
