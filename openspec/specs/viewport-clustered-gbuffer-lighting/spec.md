# viewport-clustered-gbuffer-lighting Specification

## Purpose

Shade the editor Viewport's deferred GBuffer so each pixel's point and spot lighting comes from that pixel's froxel light list rather than a single scene-wide light cap, while directional lighting keeps using the existing fullscreen deferred pass.

## Requirements

### Requirement: GBuffer lighting pass shades point/spot from the pixel's froxel
For each shaded editor Viewport pixel, the GBuffer lighting pass SHALL look up that pixel's froxel from its screen tile and reconstructed view depth (using the same tile size and exponential Z distribution as the froxel grid build) and SHALL shade that pixel's point and spot lighting using only the lights assigned to that froxel.

#### Scenario: Pixel lights from its own froxel, not a global list
- **WHEN** a shaded pixel's froxel has an assigned-light list of point and spot lights
- **THEN** that pixel's point/spot lighting contribution comes only from lights in that list

#### Scenario: Adjacent froxels can light differently
- **WHEN** two pixels map to different froxels with different assigned-light lists
- **THEN** their point/spot lighting contributions can differ even at the same view depth

### Requirement: Many-light scenes are not bounded by the flat 8-light cap in the Viewport
A scene with more than 8 Light-enabled point/spot lights affecting visible geometry SHALL show contributions from more than 8 of them in the editor Viewport, bounded only by the per-froxel cap (64) on lights sharing a froxel, not by a single global or per-MeshRenderer light count.

#### Scenario: Sponza-class scene shades beyond 8 lights
- **WHEN** a Sponza-class scene has more than 8 Light-enabled point/spot lights spread across the visible geometry such that no single froxel holds more than 64 of them
- **THEN** the editor Viewport shades contributions from more than 8 of those lights simultaneously

### Requirement: Directional lighting stays on the existing fullscreen deferred pass
The GBuffer lighting pass SHALL NOT read directional lights from the froxel grid and SHALL NOT change how directional lighting is computed. Directional lighting SHALL continue to be applied through the existing fullscreen deferred lighting pass, unscoped by any froxel.

#### Scenario: Directional lighting is unchanged by clustering
- **WHEN** the open scene has a Light-enabled Directional Light and at least one clustered point/spot light
- **THEN** the Directional Light's contribution to a shaded pixel is the same as it would be with no point/spot lights in the scene

### Requirement: Clustered point/spot lighting is unshadowed this slice
The GBuffer lighting pass's point/spot contribution from the froxel grid SHALL NOT sample or apply Light shadows this slice. Point and Spot Lights SHALL remain non-shadow-casting, consistent with existing Light shadow behavior.

#### Scenario: Clustered point light casts no shadow
- **WHEN** a Light-enabled Point Light with contribution "Illuminate and shadows" is shaded through the froxel-based GBuffer lighting pass
- **THEN** its contribution to that pixel is illumination only, with no shadow term

### Requirement: Clustered GBuffer lighting is scoped to the editor Viewport
This GBuffer lighting pass SHALL apply only to the editor Viewport's deferred render path. Player, Camera Preview, Placement Preview, Mesh Preview, and Scene Thumbnail render paths SHALL NOT use the froxel grid or this lighting pass.

#### Scenario: Player is unaffected
- **WHEN** the same scene renders in the editor Viewport and in the Player
- **THEN** only the editor Viewport's shading uses the froxel-based lighting pass
