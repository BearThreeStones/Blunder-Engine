## ADDED Requirements

### Requirement: Dedicated Mesh Preview offscreen
The editor SHALL provide a Mesh Preview Render path that draws a Mesh Asset into a dedicated offscreen render target that is not the main viewport offscreen and not the Camera Preview secondary offscreen. Frames SHALL be obtained via CPU readback for UI and thumbnail consumers.

#### Scenario: Separate from Camera Preview
- **WHEN** Mesh Preview Render produces a frame for a Mesh Asset
- **THEN** it does not render into the Camera Preview render target or the main viewport offscreen used for the editor scene view

### Requirement: Final preferred then Fast Path
Mesh Preview Render SHALL load the Mesh Asset using Final when present and fresh, otherwise Fast Path Intermediate. Mesh Preview Render SHALL NOT require a successful Cook before producing a first 3D still when Intermediate is loadable.

#### Scenario: Uncooked Mesh still works
- **WHEN** a Mesh Asset has loadable Intermediate and no fresh Final
- **THEN** Mesh Preview Render still produces a 3D still (or reports failure and allows placeholder fallback)

### Requirement: Framing lighting and submeshes
Mesh Preview Render SHALL frame the Mesh using its bounds with padding, apply fixed studio lighting (not the active scene light rig), and draw all submeshes with their materials. Shadows MAY be disabled. Skinned Meshes SHALL be drawn in bind pose (or equivalent rest pose) without AnimationPlayer playback in this slice.

#### Scenario: Multi-material Mesh is not a single texture atlas
- **WHEN** Mesh Preview Render succeeds for a Mesh with multiple materials
- **THEN** the still shows framed geometry with those materials, not solely a resized base-color texture image

### Requirement: Mesh thumbnails from Mesh Preview Render
For Content Browser entries that are Mesh Asset descriptors, Thumbnail generation SHALL prefer a Mesh Preview Render still written into the thumbnail cache. Generation SHALL be asynchronous and SHALL prioritize visible grid items. Until a still is ready, the UI MAY show a Mesh placeholder or a previous cache hit. If Mesh Preview Render fails, Thumbnail generation SHALL fall back to a Mesh placeholder (not a silent base-color texture as the primary success path).

#### Scenario: Visible-first queue
- **WHEN** the Content Browser shows a folder containing many Mesh Assets
- **THEN** thumbnails for currently visible items are generated before off-screen items

#### Scenario: Texture Assets unchanged
- **WHEN** Thumbnail generation runs for a Texture Asset descriptor
- **THEN** it continues to use the image thumbnail path, not Mesh Preview Render

### Requirement: Thumbnail cache replaces texture shortcut
When a Mesh thumbnail is successfully generated via Mesh Preview Render, the cached image SHALL be that 3D still. The system SHALL NOT treat “resize material base-color texture” as a successful Mesh thumbnail when Mesh Preview Render is available and succeeds.

#### Scenario: Regenerated Mesh thumbnail is 3D
- **WHEN** a Mesh Asset thumbnail is generated or regenerated successfully after this capability ships
- **THEN** the cache PNG is a Mesh Preview Render still, not an unframed base-color atlas used as the sole success result
