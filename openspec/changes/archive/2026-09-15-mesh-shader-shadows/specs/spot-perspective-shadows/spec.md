## Purpose

Cast Spot Light shadows from one 2D perspective depth map per shadowing spot, filled by the same opaque-meshlet depth-only path as other mesh-shader shadows and sampled with the existing hardware PCF.

## ADDED Requirements

### Requirement: Shadowing Spot lights use one 2D perspective map
Each Light-enabled Spot Light whose contribution includes shadows, whose Object is Active in Hierarchy, and who is within the per-view Spot shadow budget SHALL cast through one 2D perspective depth map whose projection matches that Spot's cone (outer angle) and Light range as far plane. The system SHALL NOT draw six cube faces for a Spot and SHALL NOT place Spot lights into the directional VSM.

#### Scenario: Spot occludes a receiver
- **WHEN** an opaque meshlet sits inside a shadowing Spot Light's cone between the light and a linked receiver
- **THEN** Viewport and Player shade that receiver as shadowed from that Spot Light

#### Scenario: Spot is not a cube
- **WHEN** a shadowing Spot Light is filled
- **THEN** the engine allocates one 2D depth map for it, not a cubemap and not VSM pages

### Requirement: Same meshlet depth-only fill
When mesh shaders are available, Spot maps SHALL be filled by the mesh-shader shadow raster (opaque cooked meshlets, depth-only). When mesh shaders are missing, they SHALL use the VS/FS shadow-depth fallback.

#### Scenario: Mesh path fills the spot map
- **WHEN** mesh shaders are available and a shadowing Spot Light has opaque meshlet casters in its frustum
- **THEN** those meshlets are rasterized depth-only into that Spot's 2D map

### Requirement: Spot shadows use existing PCF
Spot map sampling SHALL use the existing 2×2 hardware comparison PCF. It SHALL NOT switch this slice to Unreal SMRT or a software Vogel-disk filter.

#### Scenario: PCF remains
- **WHEN** a receiver samples a Spot shadow map
- **THEN** the shadow term is a 2×2 hardware comparison filter, not SMRT

### Requirement: Spot map budget
A view SHALL fill 2D maps for at most 8 shadowing Spot Lights, taken first in stable EntityId order among Light-enabled Spot Lights whose contribution includes shadows. Additional Spot Lights SHALL still illuminate when their contribution includes illumination, but SHALL NOT cast this slice. Drops SHALL be logged.

#### Scenario: Ninth shadowing Spot does not get a map
- **WHEN** nine Light-enabled Spot Lights all have contribution Illuminate and shadows
- **THEN** only the first 8 in stable EntityId order fill 2D shadow maps
- **AND** the ninth still adds direct light
