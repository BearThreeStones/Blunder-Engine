# Spec Delta

## MODIFIED Requirements

### Requirement: Player does not draw Editor Overlays
While running as Player, the system SHALL NOT draw ground grid, Transform gizmo, Navigate gizmo, selection outline, world axes, origins, wireframe, Camera Gizmo, Light Gizmo, or collision wireframe Editor Overlays. Play Pause SHALL NOT re-enable those draws.

#### Scenario: Player frame has no authorship chrome
- **WHEN** the Player presents a frame (including while paused)
- **THEN** Editor Overlay authorship chrome is not drawn
