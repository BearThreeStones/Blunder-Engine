## MODIFIED Requirements

### Requirement: Record-path bindings must match exactly
A graphics pipeline on the shared path SHALL NOT finish initialization unless the Shader resource layout’s binding set is exactly the set of descriptor bindings that pipeline’s record path writes. A mismatch SHALL abort process start. Partial or extra bindings SHALL NOT be ignored. The Sobel VRS compute SHALL be on that exact-match path (same family as `froxel_fill`) and SHALL NOT bind the Bindless table.

#### Scenario: Extra shader binding fails start
- **WHEN** a shared-path Engine shader declares a descriptor binding that the record path does not write
- **THEN** process start fails before a viewport is presented

#### Scenario: Missing record binding fails start
- **WHEN** the record path writes a descriptor binding that the compiled Engine shader does not declare
- **THEN** process start fails before a viewport is presented

#### Scenario: Matching bindings allow start
- **WHEN** the compiled Engine shader’s binding set equals the record path’s writes
- **THEN** the graphics pipeline initializes
- **AND** the editor can present the viewport

## ADDED Requirements

### Requirement: Sobel VRS compute is exact-match
The Sobel VRS compute pipeline SHALL derive its Pipeline layout from the compiled Engine shader and SHALL NOT finish initialization unless that layout equals the record path’s descriptor writes. It SHALL NOT bind the Bindless table.

#### Scenario: Sobel compute layout matches the record path
- **WHEN** the engine creates the VRS Sobel compute pipeline from its Engine shader
- **THEN** that pipeline’s Pipeline layout bindings are exactly the resource bindings that compiled shader declares
- **AND** the record path writes that same set
- **AND** the compute does not bind the Bindless table
