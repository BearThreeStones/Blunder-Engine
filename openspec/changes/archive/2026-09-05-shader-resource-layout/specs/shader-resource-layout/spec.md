## Purpose

Derive the GPU resource interface of a compiled Engine shader and use it as the Pipeline layout for the shared graphics-pipeline path, so CPU feature flags cannot drift from that shader.

## ADDED Requirements

### Requirement: Shader resource layout comes from the compiled Engine shader
When the shared graphics-pipeline path compiles an Engine shader for a graphics pipeline, it SHALL derive that shader’s Shader resource layout from the same compile that produces GPU bytecode. The Pipeline layout for that pipeline SHALL be created from that Shader resource layout. The Shader resource layout SHALL NOT be an Asset, SHALL NOT be Cook output, and SHALL NOT be persisted in this change.

#### Scenario: Mesh pipeline layout matches the Engine shader
- **WHEN** the editor creates the forward mesh graphics pipeline from `pbr.slang`
- **THEN** that pipeline’s Pipeline layout bindings are exactly the resource bindings that compiled `pbr.slang` declares
- **AND** CPU feature flags do not author those bindings

#### Scenario: Skinned mesh includes the bone palette binding
- **WHEN** the editor creates the skinned mesh graphics pipeline from `pbr_skinned.slang`
- **THEN** that pipeline’s Pipeline layout includes the bone-palette uniform binding declared by that shader
- **AND** the unskinned mesh pipeline from `pbr.slang` does not include that binding

### Requirement: Record-path bindings must match exactly
A graphics pipeline on the shared path SHALL NOT finish initialization unless the Shader resource layout’s binding set is exactly the set of descriptor bindings that pipeline’s record path writes. A mismatch SHALL abort process start. Partial or extra bindings SHALL NOT be ignored.

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

### Requirement: Same Engine shader may share one Pipeline layout
Two graphics pipelines that compile the same Engine shader and differ only in raster state (blend, depth write, cull) SHALL use one Shader resource layout / Pipeline layout rather than authoring a second binding table.

#### Scenario: Transparent mesh shares opaque layout
- **WHEN** the transparent mesh pipeline is created from the same Engine shader as the opaque mesh pipeline
- **THEN** it uses that opaque pipeline’s Pipeline layout
- **AND** it still enables blending and disables depth write

### Requirement: Shared-path visuals stay correct
After Shader resource layout generation, the editor viewport SHALL still draw forward meshes (opaque, transparent, shadowed, skinned) with textures in the declared slots. Mesh Preview SHALL still frame and shade a Mesh Asset. Ground grid, Transform gizmo, and Navigate gizmo SHALL still draw. One-off outline, SSAO, pick, and pick-compute passes SHALL keep their existing hand-written layouts.

#### Scenario: Viewport PBR scene
- **WHEN** the author opens the Test Project with a scene that uses PBR mesh materials
- **THEN** opaque and transparent meshes, shadows when enabled, and skinned meshes draw with textures in the correct slots

#### Scenario: Mesh Preview
- **WHEN** the author opens Mesh Preview for a Mesh Asset that has material textures
- **THEN** the preview still auto-frames
- **AND** the surface matches that Mesh Asset’s materials

#### Scenario: Grid and gizmos
- **WHEN** the editor viewport is shown
- **THEN** the ground grid, Transform gizmo (with a selection), and Navigate gizmo still draw

#### Scenario: One-off passes unchanged
- **WHEN** the editor runs outline, SSAO, viewport pick, or pick compute
- **THEN** those passes still use their existing hand-written descriptor layouts
