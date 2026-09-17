# mesh-shader-shadow-raster Specification

## Purpose

Fill Viewport and Player shadow targets with a depth-only mesh/task raster of opaque cooked GPU-driven meshlets, falling back to the existing VS/FS shadow-depth path when mesh shaders are unavailable.

## Requirements

### Requirement: Opaque cooked meshlets are the only mesh-shader casters
The mesh-shader shadow raster SHALL draw only opaque meshlets from the existing cooked GPU-driven meshlet buffers. It SHALL NOT recook meshlets. It SHALL NOT emit alpha-masked, transparent, skinned, or foliage meshlets as shadow casters.

#### Scenario: Opaque meshlet casts
- **WHEN** Viewport or Player renders a Light-enabled Directional whose contribution includes shadows and an opaque GPU-driven meshlet is in that light's caster set
- **THEN** that meshlet is recorded into the mesh-shader shadow raster

#### Scenario: Alpha meshlet does not cast
- **WHEN** a meshlet is flagged alpha-mask or transparent
- **THEN** the mesh-shader shadow raster does not emit that meshlet

#### Scenario: Skinned and foliage do not cast
- **WHEN** the only scene geometry is skinned or foliage
- **THEN** the mesh-shader shadow raster emits no casters

### Requirement: Mesh shaders fill; VS/FS is fallback
When the device supports `VK_EXT_mesh_shader` (meshShader feature), Viewport and Player SHALL fill shadow targets with a depth-only mesh and task shader path (positions only; no material fragment shader on that technique). When that support is missing, they SHALL keep the existing VS/FS shadow-depth path instead of failing to start or showing a blank view.

#### Scenario: Mesh shaders present
- **WHEN** the device exposes mesh shaders and opaque meshlet casters exist
- **THEN** Viewport and Player fill shadow targets with the mesh/task depth-only path

#### Scenario: Mesh shaders missing
- **WHEN** the device does not expose mesh shaders
- **THEN** Viewport and Player still produce shadows using the VS/FS shadow-depth path
- **AND** the process does not abort at device init solely for that absence

### Requirement: Viewport and Player share the caster recording
The editor Viewport and the Player SHALL use the same meshlet caster recording and the same shadow-map topology for a given scene and camera. Camera Preview SHALL remain shadows-off. Mesh Preview, Placement Preview, and Scene Thumbnail SHALL NOT gain this mesh-shader shadow pass.

#### Scenario: Player matches Viewport casters
- **WHEN** the same scene has a shadowing Directional and opaque meshlets
- **THEN** both the editor Viewport and the Player show that Directional's mesh-shader (or fallback) shadows

#### Scenario: Camera Preview stays off
- **WHEN** Camera Preview is visible
- **THEN** it does not run the mesh-shader shadow raster and does not rewrite Viewport shadow maps
