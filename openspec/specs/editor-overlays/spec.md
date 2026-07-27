# editor-overlays Specification

## Purpose
TBD - created by archiving change player-hide-editor-overlays. Update Purpose after archive.
## Requirements
### Requirement: Editor Overlays enabled only for Editor host
The system SHALL enable Editor Overlay draw and hit-testing when the engine host mode is Editor. The system SHALL disable Editor Overlay draw and hit-testing when the engine host mode is Player.

#### Scenario: Editor host enables overlays
- **WHEN** `EngineHostMode` is `Editor`
- **THEN** `editorOverlaysEnabled` is true

#### Scenario: Player host disables overlays
- **WHEN** `EngineHostMode` is `Player`
- **THEN** `editorOverlaysEnabled` is false

### Requirement: Player does not draw Editor Overlays
While running as Player, the system SHALL NOT draw ground grid, Transform gizmo, Navigate gizmo, selection outline, world axes, origins, or wireframe Editor Overlays. Play Pause SHALL NOT re-enable those draws.

#### Scenario: Player frame has no authorship chrome
- **WHEN** the Player presents a frame (including while paused)
- **THEN** Editor Overlay authorship chrome is not drawn

### Requirement: Player ignores Editor Overlay gizmo input
While running as Player, the system SHALL NOT handle Transform gizmo or Navigate gizmo pointer interaction. Editor Camera orbit MAY still operate.

#### Scenario: Navigate gizmo click ignored in Player
- **WHEN** the author clicks where the Navigate gizmo would be in the Player window
- **THEN** the click is not handled as Navigate gizmo input

