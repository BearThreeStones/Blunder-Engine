## MODIFIED Requirements

### Requirement: Shared-path visuals stay correct
After Shader resource layout generation, the editor viewport SHALL still draw forward meshes (opaque, transparent, shadowed, skinned) with textures in the declared slots. Mesh Preview SHALL still frame and shade a Mesh Asset. Ground grid, Transform gizmo, and Navigate gizmo SHALL still draw. One-off outline, SSAO, pick, pick-compute, and volumetric-fog compute/composite passes SHALL keep their existing hand-written layouts.

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
- **WHEN** the editor runs outline, SSAO, viewport pick, or pick compute, or the Player runs volumetric-fog compute and composite
- **THEN** those passes still use their existing hand-written descriptor layouts
