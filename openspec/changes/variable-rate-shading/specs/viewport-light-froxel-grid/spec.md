## ADDED Requirements

### Requirement: Froxel grid builds for deferred surfaces
The froxel grid SHALL be built once per rendered frame for each surface that uses clustered GBuffer lighting (editor Viewport, Player, Camera Preview, Mesh Preview Render, Scene Thumbnail Render, Capture). Placement Preview SHALL NOT build this grid. Occupancy heatmap visualisation SHALL remain editor Viewport only.

#### Scenario: Player frame fills froxels
- **WHEN** the Player records a deferred frame with at least one Light-enabled Point Light
- **THEN** that frame’s froxel fill compute SHALL run for the Player offscreen extent
