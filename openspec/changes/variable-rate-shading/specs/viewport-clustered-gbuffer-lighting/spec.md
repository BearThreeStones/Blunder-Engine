## MODIFIED Requirements

### Requirement: Clustered GBuffer lighting is scoped to the editor Viewport
This GBuffer lighting pass SHALL apply to every surface that uses the Deferred Render Path in this change (editor Viewport, Player, Camera Preview, Mesh Preview Render, Scene Thumbnail Render, Capture). Placement Preview SHALL NOT use the froxel grid or this lighting pass. Image-based VRS, when enabled, SHALL attach to this lighting fullscreen triangle and SHALL NOT replace froxel lookup.

#### Scenario: Player is unaffected
- **WHEN** the same scene renders in the editor Viewport and in the Player
- **AND** both use the Deferred Render Path
- **THEN** both surfaces’ point/spot shading SHALL use the froxel-based lighting pass
