# Spec Delta

## MODIFIED Requirements

### Requirement: Pause skips Behaviour Tick
While paused, the Player SHALL skip gameplay Behaviour Tick (and equivalent gameplay simulation time) and SHALL skip Physics World steps, but SHALL keep the process alive (and, when windowed, keep the window alive so the world remains viewable). Resume SHALL continue Tick and physics steps from the paused world state.

#### Scenario: Pause then Resume
- **WHEN** the Player receives pause then later resume
- **THEN** Tick does not advance during pause and resumes afterward without requiring a process restart

#### Scenario: Pause skips physics steps
- **WHEN** the Player is paused
- **THEN** the Play Process Physics World does not step

## ADDED Requirements

### Requirement: Player hosts the scene Physics World
`engine_player` SHALL host the Play Process SceneInstance Physics World so C# physics queries and Character Controller MoveAndSlide run in Play, including Headless Play. Host or Scripts failure SHALL remain non-fatal to process start when the scene can still load.

#### Scenario: Play ray works in Player
- **WHEN** the Player is running a scene with a Static Collider Unique and a Behaviour raycasts that collider
- **THEN** the query can hit
