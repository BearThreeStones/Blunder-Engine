## Purpose

Cast Point Light shadows from a 6-face cubemap filled by the mesh-shader depth-only path (one layered dispatch when Vulkan allows, otherwise six face passes) and sampled with the existing hardware PCF.

## ADDED Requirements

### Requirement: Shadowing Point lights use a 6-face cubemap
Each Light-enabled Point Light whose contribution includes shadows, whose Object is Active in Hierarchy, and who is within the per-view Point shadow budget SHALL cast through a 6-face cubemap (or a 6-layer 2D array representing cube faces). The system SHALL NOT use a Nanite visbuffer plus six emit copies to fill those faces.

#### Scenario: Point occludes a receiver
- **WHEN** an opaque meshlet sits between a shadowing Point Light and a linked receiver inside Light range
- **THEN** Viewport and Player shade that receiver as shadowed from that Point Light

#### Scenario: Illuminate only does not cast
- **WHEN** a Point Light's contribution is Illuminate only
- **THEN** it does not fill a cubemap

### Requirement: Layered mesh dispatch when Vulkan allows, else six passes
When the device supports writing a render-target array index from the mesh shader, the Point cubemap fill SHALL use a layered mesh dispatch that selects the cube face per primitive. When that layered output is unavailable, the fill SHALL use six face passes. Both paths SHALL use the mesh-shader shadow raster when mesh shaders are present.

#### Scenario: Layered path
- **WHEN** mesh shaders and mesh-shader array-index output are available
- **THEN** a shadowing Point Light's six faces are filled without a Nanite-style per-face visbuffer emit

#### Scenario: Six-pass fallback
- **WHEN** mesh shaders are available but mesh-shader array-index output is not
- **THEN** the engine still fills all six cube faces via six passes
- **AND** Viewport and Player still show that Point Light's shadows

### Requirement: Point shadows use existing PCF
Point cubemap sampling SHALL use the existing 2×2 hardware comparison PCF. It SHALL NOT switch this slice to Unreal SMRT or a software Vogel-disk filter.

#### Scenario: PCF remains
- **WHEN** a receiver samples a Point cubemap
- **THEN** the shadow term is a 2×2 hardware comparison filter, not SMRT

### Requirement: Point cubemap budget
A view SHALL fill cubemaps for at most 8 shadowing Point Lights, taken first in stable EntityId order among Light-enabled Point Lights whose contribution includes shadows. Additional Point Lights SHALL still illuminate when their contribution includes illumination, but SHALL NOT cast this slice. Drops SHALL be logged.

#### Scenario: Ninth shadowing Point does not get a map
- **WHEN** nine Light-enabled Point Lights all have contribution Illuminate and shadows
- **THEN** only the first 8 in stable EntityId order fill cubemaps
- **AND** the ninth still adds direct light
