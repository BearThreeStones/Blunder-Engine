## Purpose

Player game views composite camera-aligned volumetric fog on the existing Forward path so distant meshes and empty sky participate in media, without a new GBuffer or clustered local-light inject.

## ADDED Requirements

### Requirement: Player Forward composite
When an active Fog Component is present, the Player color target SHALL composite volumetric fog after Forward lighting as `color * T + inscatter`, using the Play-rule camera. Windowed Player and Headless Play frames SHALL both show that composite. Empty sky / background pixels SHALL receive the far-slice lookup so unfogged clear color does not remain. The editor Viewport, Camera Preview, Placement Preview, Mesh Preview, and Scene Thumbnail SHALL NOT be required to show volumetric fog this slice.

#### Scenario: Player shows fog
- **WHEN** Play is running, the entry scene has an active Fog Component, and a Directional Light illuminates the scene
- **THEN** the Player game view shows distance fog on opaque meshes and on empty sky via `color * T + inscatter`

#### Scenario: Viewport may stay clear
- **WHEN** the same scene is shown in the editor Viewport
- **THEN** that Viewport is allowed to have no volumetric fog this slice

#### Scenario: Headless Play frame is fogged
- **WHEN** a Headless Player has an active Fog Component and a Play frame is requested
- **THEN** the Play frame color still includes the volumetric-fog composite

### Requirement: Camera-aligned froxel volume
Volumetric fog SHALL live in a camera-aligned 3D volume for the Player view: XY cells of about **16 pixels** on the view rect, **64** Z slices, exponential view-depth with distribution scale **S = 32**. Slice range SHALL run from a small near offset through Fog view distance, not a separate world AABB. Density at each cell SHALL use exponential height falloff along world **Z** (up) relative to the active Fog entity’s world Z. The engine SHALL NOT run a separate 2D analytical height-fog pass this slice.

#### Scenario: Tile and slice counts
- **WHEN** the Player view is 1920×1080 and Fog view distance is 60 meters
- **THEN** the volume XY size is ceil(view extent / 16) and Z size is 64

#### Scenario: Height uses world Z
- **WHEN** two froxels share XY and differ only in world Z
- **THEN** the higher-Z cell has lower or equal height density under positive falloff
- **AND** world Y is not the height axis

#### Scenario: No 2D height-fog pass
- **WHEN** volumetric fog is compositing on the Player
- **THEN** there is no additional fullscreen analytical height-fog pass

### Requirement: Unshadowed directional inject and thin temporal
Light scattering into the volume SHALL include the first Light enabled Directional whose contribution includes illumination, in stable EntityId order, with volumetric shadow factor 1. Point lights, spot lights, and area lights SHALL NOT inject this slice. The engine SHALL NOT sample shadow maps, virtual shadow pages, or extra volume-shadow rays for this fog. Temporal reprojection SHALL blend about **20% current / 80% history** on the pre-integrated scatter volume. The engine SHALL NOT apply a sticky `max(history, current)` highlight keep.

#### Scenario: Directional injects without shadows
- **WHEN** an active Fog and one illuminating Directional exist in the Player scene
- **THEN** that Directional contributes in-scatter in the volume
- **AND** shadow maps do not darken froxels

#### Scenario: Locals do not inject
- **WHEN** the scene also has Point and Spot Lights
- **THEN** those lights do not add in-scatter in the volume this slice

#### Scenario: Thin temporal is on
- **WHEN** the Player camera is still and Fog is active
- **THEN** successive frames blend about 20% of the current scatter volume with history

### Requirement: Stack on existing Forward Player
Volumetric fog SHALL stack on the existing Forward Player mesh path. The engine SHALL NOT add a GBuffer, deferred lighting path, or clustered light list in order to ship this fog. Volume resources SHALL stay off the Bindless color table.

#### Scenario: No new GBuffer
- **WHEN** Player volumetric fog is enabled
- **THEN** Forward opaque and transparent mesh draws still write the Player color and depth targets
- **AND** no GBuffer attachments are required for the fog composite
