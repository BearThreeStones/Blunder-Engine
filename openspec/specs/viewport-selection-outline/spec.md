# viewport-selection-outline

## Purpose

Blender-style silhouette outline for selected mesh entities in the 3D viewport (ID prepass + edge resolve).

## Requirements

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

### Requirement: Transparent selected meshes included

The outline prepass SHALL include selected entities whose mesh renderer uses blend (`cgltf_alpha_mode_blend`) transparency, drawing the mesh surface geometry into the outline ID buffer.

#### Scenario: Select transparent mesh entity

- **WHEN** the user selects an entity with a blend-mode mesh renderer
- **THEN** the viewport SHALL display the same orange silhouette outline around that mesh's surface hull

### Requirement: Default-enabled outline

The selection outline feature SHALL be active by default whenever selection preconditions are met. No user preference toggle or environment variable gate is required for v1.

#### Scenario: Fresh editor session

- **WHEN** the user launches the editor and selects a mesh entity without changing any settings
- **THEN** the orange outline SHALL appear without additional configuration

### Requirement: Blender-style orange outline color

The resolve pass SHALL composite outline pixels using hardcoded Blender orange colors: object-select orange (approximately `#F57011`) when not transform-dragging, and transform orange when the transform gizmo is actively dragging.

#### Scenario: Outline color on visible edge

- **WHEN** a silhouette edge pixel is detected on an unoccluded surface and the gizmo is not dragging
- **THEN** the outline color SHALL match the hardcoded Blender object-select orange

#### Scenario: Transform-drag color

- **WHEN** a silhouette edge pixel is detected while the transform gizmo is dragging
- **THEN** the outline color SHALL match the hardcoded Blender transform orange

### Requirement: Depth-based outline occlusion

When an outline edge is behind other scene geometry (outline depth greater than scene depth beyond a small epsilon), the outline SHALL render at reduced opacity (default alpha factor `0.35`).

#### Scenario: Outline behind foreground object

- **WHEN** part of the selected mesh's silhouette is occluded by another mesh closer to the camera
- **THEN** the occluded outline segments SHALL appear semi-transparent relative to fully visible outline segments

### Requirement: Pipeline integration without forward-pass changes

The outline passes SHALL run after the forward scene render pass completes and before overlay line anti-aliasing, without modifying PBR forward shaders or the main opaque draw list structure.

#### Scenario: Frame render order

- **WHEN** the render system records a viewport frame with a selected mesh entity
- **THEN** the engine SHALL execute forward opaque/transparent passes first, then outline prepass and resolve, then subsequent overlay phases (lines, AA, SSAO, screen overlays)

### Requirement: Selection overlay updates after viewport pick

When viewport pick changes the primary selection, the outline and transform gizmo SHALL become visible without requiring camera movement and SHALL remain visible until selection changes or clears.

#### Scenario: Outline after first left-click with static camera

- **WHEN** the user left-clicks to select a mesh entity (sync-then-async path) and does not move the editor camera
- **THEN** the orange selection outline SHALL appear on the next viewport frame after click release without requiring a camera move or a second click

#### Scenario: Transform gizmo after first left-click with static camera

- **WHEN** the user left-clicks to select a mesh entity (sync-then-async path) and does not move the editor camera
- **THEN** the transform gizmo SHALL appear on the next viewport frame after click release without requiring a camera move or a second click

#### Scenario: Outline persists after piercing menu selection

- **WHEN** the user selects an entity from the piercing menu
- **THEN** the outline and transform gizmo SHALL appear and SHALL remain visible on subsequent static-camera frames without requiring another click or camera move

### Requirement: Selection change triggers viewport composite dirty

When the editor primary selection changes, the UI viewport presentation layer SHALL be notified to refresh the viewport image region.

#### Scenario: Slint dirty on selection

- **WHEN** `EditorSelectionSystem` changes the primary selection to a valid mesh-bearing entity
- **THEN** the engine SHALL mark the Slint viewport dirty region in addition to requesting a viewport offscreen redraw
