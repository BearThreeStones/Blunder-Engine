## ADDED Requirements

### Requirement: Selected entity outline visibility

When the editor has a valid selected entity with a mesh renderer, the 3D viewport SHALL render a visible silhouette outline around that entity's mesh surface using a two-pass GPU outline (ID prepass + edge resolve).

#### Scenario: Select opaque mesh entity

- **WHEN** the user selects an entity that has a `MeshRendererComponent` with an opaque material
- **THEN** the viewport SHALL display an orange outline along the silhouette edges of that mesh

#### Scenario: No selection

- **WHEN** the editor has no selected entity (`EditorSelectionSystem::hasSelection()` is false)
- **THEN** the outline pass SHALL NOT draw any outline pixels

#### Scenario: Selection without mesh

- **WHEN** the selected entity has no `MeshRendererComponent` or no valid `GpuMesh`
- **THEN** the outline pass SHALL be disabled for that frame

#### Scenario: Select parent entity with mesh children

- **WHEN** the user selects a parent entity that has no direct `MeshRendererComponent` but has descendant entities with valid mesh renderers (e.g. glTF `*_primN` children)
- **THEN** the viewport SHALL display an orange outline around all mesh surfaces in that entity's subtree

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

The resolve pass SHALL composite outline pixels using a hardcoded Blender object-select orange color (approximately RGB `(245, 112, 17)` / `#F57011`).

#### Scenario: Outline color on visible edge

- **WHEN** a silhouette edge pixel is detected on an unoccluded surface
- **THEN** the outline color SHALL match the hardcoded Blender orange (not theme- or user-configurable in v1)

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
