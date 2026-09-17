## MODIFIED Requirements

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
