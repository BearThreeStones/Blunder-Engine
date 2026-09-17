# bindless-texture-table Specification

## Purpose
Give every mesh shading path on a Vulkan device one resident sampled-image and sampler table so color material textures are selected by stable index instead of a per-draw descriptor set.

## Requirements

### Requirement: One Bindless texture table per device
The process SHALL own one Bindless texture table per Vulkan device. Every mesh shading path on that device (editor viewport, Mesh Preview, Camera Preview, Scene Thumbnail / Capture, and Player in that process) SHALL use that same table. Overlay, SSAO, pick, and deferred lighting passes SHALL NOT use the table. The table SHALL NOT be an Asset and SHALL NOT be Cook output.

#### Scenario: Viewport meshes share the table with previews
- **WHEN** the author views several meshes with material textures in the editor viewport and then opens Mesh Preview and Camera Preview in the same session
- **THEN** those paths sample color textures from the same device table
- **AND** the meshes still show the correct surfaces

#### Scenario: Overlay stays off the table
- **WHEN** the editor draws outline, SSAO, or pick
- **THEN** those passes do not read the Bindless texture table

#### Scenario: Deferred G-buffer uses the table; lighting does not
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** an opaque mesh has a material color texture
- **THEN** the G-buffer geometry pass samples that texture from the Bindless texture table
- **AND** the deferred lighting pass does not read the table


### Requirement: Mesh draws select color textures by stable index
A mesh draw SHALL select color material sampled images and samplers by a stable table index that remains valid while that GPU texture remains loaded. The process SHALL NOT bind a new per-draw descriptor set for those color textures. Per-draw constant buffers SHALL remain a normal descriptor set. Shadow resources (directional VSM page table and physical pages, Point cubemaps, Spot 2D maps, and the VS/FS fallback directional map) SHALL stay dedicated comparison and/or page-table bindings and SHALL NOT be entries in the Bindless color table.

#### Scenario: Several materials look as before
- **WHEN** the author opens a scene with several meshes and several material textures
- **THEN** the viewport looks the same as before this table
- **AND** shadows still use dedicated comparison or page-table sampling, not the color table
- **AND** those shadow resources are not entries in the table

#### Scenario: Index stays valid while the texture is loaded
- **WHEN** a GPU texture is already in the table and a later mesh draw in that process uses that same texture
- **THEN** the draw uses the same table index
- **AND** the process does not rebuild the table from the draw list for that frame

### Requirement: Full table uses fallback and does not fail start
When unique color material textures exceed table capacity, the process SHALL assign the fallback index to the extras, SHALL log, and SHALL NOT abort process start. Missing descriptor-indexing device support SHALL fail device initialization (no per-draw texture-set fallback path).

#### Scenario: Overflow uses fallback
- **WHEN** unique color material textures exceed the table capacity
- **THEN** extra textures sample the fallback texture
- **AND** the editor still starts
- **AND** the process does not abort because the table is full

#### Scenario: Device without descriptor indexing does not start
- **WHEN** the selected GPU does not support the descriptor indexing required for this table
- **THEN** device initialization fails
- **AND** the process does not keep a per-draw color-texture descriptor path

### Requirement: Forward mesh draw cap is unchanged
The Bindless texture table SHALL NOT raise the Forward mesh draw cap. Overflow of mesh draws in a CPU list in a frame SHALL still truncate the same way as before this table. GPU-driven rendering of static opaque and alpha-clip MeshRenderers SHALL NOT be truncated by that cap.

#### Scenario: Draw cap still 256
- **WHEN** a frame records more CPU-list mesh draws than the Forward mesh draw cap
- **THEN** extra CPU-list draws are truncated the same way as before this table
- **AND** unique texture count in the table is not used as a second draw cap

#### Scenario: GPU-driven static is not that cap
- **WHEN** a frame submits more than 256 GPU-driven static opaque MeshRenderers
- **THEN** those MeshRenderers are not truncated by the Forward mesh draw cap
- **AND** they still sample color textures from the Bindless texture table
