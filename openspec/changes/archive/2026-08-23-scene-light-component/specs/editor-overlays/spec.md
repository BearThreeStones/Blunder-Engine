## MODIFIED Requirements

### Requirement: Player does not draw Editor Overlays
While running as Player, the system SHALL NOT draw ground grid, Transform gizmo, Navigate gizmo, selection outline, world axes, origins, wireframe, Camera Gizmo, or Light Gizmo Editor Overlays. Play Pause SHALL NOT re-enable those draws.

#### Scenario: Player frame has no authorship chrome
- **WHEN** the Player presents a frame (including while paused)
- **THEN** Editor Overlay authorship chrome is not drawn

### Requirement: Player ignores Editor Overlay gizmo input
While running as Player, the system SHALL NOT handle Transform gizmo, Navigate gizmo, Camera Gizmo, or Light Gizmo pointer interaction. Editor Camera orbit MAY still operate.

#### Scenario: Navigate gizmo click ignored in Player
- **WHEN** the author clicks where the Navigate gizmo would be in the Player window
- **THEN** the click is not handled as Navigate gizmo input

#### Scenario: Light Gizmo click ignored in Player
- **WHEN** the pointer clicks where a Light Gizmo would be in the Player window
- **THEN** the click is not handled as Light Gizmo input
