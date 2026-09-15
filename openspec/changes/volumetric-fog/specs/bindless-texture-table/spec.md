## MODIFIED Requirements

### Requirement: One Bindless texture table per device
The process SHALL own one Bindless texture table per Vulkan device. Every mesh shading path on that device (editor viewport, Mesh Preview, Camera Preview, Scene Thumbnail / Capture, and Player in that process) SHALL use that same table. Overlay, SSAO, pick, and volumetric-fog passes SHALL NOT use the table. The table SHALL NOT be an Asset and SHALL NOT be Cook output.

#### Scenario: Viewport meshes share the table with previews
- **WHEN** the author views several meshes with material textures in the editor viewport and then opens Mesh Preview and Camera Preview in the same session
- **THEN** those paths sample color textures from the same device table
- **AND** the meshes still show the correct surfaces

#### Scenario: Overlay stays off the table
- **WHEN** the editor draws outline, SSAO, or pick, or the Player runs volumetric fog
- **THEN** those passes do not read the Bindless texture table
