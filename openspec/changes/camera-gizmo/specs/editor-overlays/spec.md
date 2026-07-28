## MODIFIED Requirements

### Requirement: Player does not draw Editor Overlays

While running as Player, the system SHALL NOT draw ground grid, Transform gizmo, Navigate gizmo, selection outline, world axes, origins, wireframe, or **Camera Gizmo** Editor Overlays. Play Pause SHALL NOT re-enable those draws.

#### Scenario: Player frame has no authorship chrome

- **WHEN** the Player presents a frame (including while paused)
- **THEN** Editor Overlay authorship chrome (including Camera Gizmo) is not drawn

### Requirement: Player ignores Editor Overlay gizmo input

While running as Player, the system SHALL NOT handle Transform gizmo, Navigate gizmo, or **Camera Gizmo** pointer interaction.

#### Scenario: Camera Gizmo click ignored in Player

- **WHEN** the author clicks where a Camera Gizmo would be in the Player window
- **THEN** the click is not handled as Camera Gizmo input
