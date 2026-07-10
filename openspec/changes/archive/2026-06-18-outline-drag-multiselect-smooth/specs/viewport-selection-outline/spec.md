## MODIFIED Requirements

### Requirement: Selected entity outline visibility

When the editor has one or more valid selected entities with mesh renderers (directly or in their subtrees), the 3D viewport SHALL render a visible silhouette outline around those mesh surfaces using a two-pass GPU outline (ID prepass + edge resolve).

#### Scenario: Select opaque mesh entity

- **WHEN** the user selects an entity that has a `MeshRendererComponent` with an opaque material (or a subtree containing one)
- **THEN** the viewport SHALL display an orange outline along the silhouette edges of that mesh

#### Scenario: No selection

- **WHEN** the editor has no selected entities (`EditorSelectionSystem::hasSelection()` is false)
- **THEN** the outline pass SHALL NOT draw any outline pixels

#### Scenario: Selection without mesh

- **WHEN** every selected entity has no `MeshRendererComponent` and no descendant with a valid `GpuMesh`
- **THEN** the outline pass SHALL be disabled for that frame

#### Scenario: Select parent entity with mesh children

- **WHEN** the user selects a parent entity that has no direct `MeshRendererComponent` but has descendant entities with valid mesh renderers (e.g. glTF `*_primN` children)
- **THEN** the viewport SHALL display an orange outline around all mesh surfaces in that entity's subtree

#### Scenario: Multiple selected entities

- **WHEN** the user selects more than one entity that each has outline-eligible mesh geometry
- **THEN** the viewport SHALL display outlines for all selected entities in the same frame

### Requirement: Blender-style orange outline color

The resolve pass SHALL composite outline pixels using hardcoded Blender orange colors: object-select orange (approximately `#F57011`) when not transform-dragging, and transform orange when the transform gizmo is actively dragging.

#### Scenario: Outline color on visible edge

- **WHEN** a silhouette edge pixel is detected on an unoccluded surface and the gizmo is not dragging
- **THEN** the outline color SHALL match the hardcoded Blender object-select orange

#### Scenario: Transform-drag color

- **WHEN** a silhouette edge pixel is detected while the transform gizmo is dragging
- **THEN** the outline color SHALL match the hardcoded Blender transform orange
