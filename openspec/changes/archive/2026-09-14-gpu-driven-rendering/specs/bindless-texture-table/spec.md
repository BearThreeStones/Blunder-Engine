## MODIFIED Requirements

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
