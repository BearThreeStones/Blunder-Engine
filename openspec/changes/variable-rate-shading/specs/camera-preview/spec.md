## MODIFIED Requirements

### Requirement: Live overlay-free preview image
While Camera Preview is visible and not collapsed, the editor SHALL render the scene each frame from the target Camera Component’s world pose, vertical FOV, near clip, and far clip into a dedicated offscreen target using the Deferred Render Path, and present that image in the panel content area. The preview render SHALL NOT draw Editor Overlays (grid, gizmos, outline, Camera Gizmo, etc.). The preview render SHALL NOT draw the VRS rate-mask overlay. Projection aspect SHALL equal the preview content box width/height. The offscreen longest edge SHALL be at most 480 pixels. When the panel is collapsed, preview rendering SHALL stop. Camera Preview SHALL remain unshadowed. Image-based VRS MAY attach to that preview’s deferred lighting pass when the device supports it; missing extension SHALL still produce the preview.

#### Scenario: Move camera updates preview
- **WHEN** Camera Preview is visible and the target Camera entity’s transform or FOV changes
- **THEN** the preview image updates to match
- **AND** that image is produced by the Deferred Render Path

#### Scenario: Collapsed stops render
- **WHEN** the user collapses Camera Preview
- **THEN** the content image is hidden and secondary render does not run
