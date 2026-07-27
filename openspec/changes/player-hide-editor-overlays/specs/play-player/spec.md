## ADDED Requirements

### Requirement: Player is not an Editor Overlay surface
The Player SHALL NOT draw or hit-test Editor Overlays (authorship viewport chrome including ground grid, Transform gizmo, Navigate gizmo, and related overlays). The editor viewport SHALL keep Editor Overlays while a Play Session is running.

#### Scenario: Play shows gameplay without authorship chrome
- **WHEN** Play Mode runs in the Player process
- **THEN** the Player window shows the game view without Editor Overlay authorship chrome
