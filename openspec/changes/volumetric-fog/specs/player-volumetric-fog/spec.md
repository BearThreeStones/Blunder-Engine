## Purpose

Editor Viewport and Player game views composite camera-aligned volumetric fog on the existing present path so distant meshes and empty sky participate in media, without a new GBuffer or clustered local-light inject.

## ADDED Requirements

### Requirement: Viewport and Player composite
When an active Fog Component is present, the presented color target SHALL composite volumetric fog as `color * T + inscatter` after lighting writes that color. Player SHALL use the Play-rule camera on the Forward path. Editor Viewport SHALL use the Viewport camera; when Viewport is Deferred, lighting SHALL still write the imported present color and depth, and fog SHALL composite on that same color — the engine SHALL NOT add fog GBuffer attachments. Windowed Player and Headless Play frames SHALL both show that composite. Empty sky / background pixels SHALL receive the far-slice lookup so unfogged clear color does not remain. Camera Preview, Placement Preview, Mesh Preview, and Scene Thumbnail SHALL NOT be required to show volumetric fog this slice.

#### Scenario: Player shows fog
- **WHEN** Play is running, the entry scene has an active Fog Component, and a Directional Light illuminates the scene
- **THEN** the Player game view shows distance fog on opaque meshes and on empty sky via `color * T + inscatter`

#### Scenario: Viewport shows the same composite
- **WHEN** the same scene is shown in the editor Viewport without Play
- **THEN** that Viewport shows volumetric fog via the same `color * T + inscatter` composite
- **AND** if Viewport is Deferred, fog composites on the presented color after deferred lighting rather than on a new GBuffer

#### Scenario: Headless Play frame is fogged
- **WHEN** a Headless Player has an active Fog Component and a Play frame is requested
- **THEN** the Play frame color still includes the volumetric-fog composite

#### Scenario: Previews stay unfogged
- **WHEN** Camera Preview, Placement Preview, Mesh Preview, or Scene Thumbnail renders a scene that has an active Fog Component
- **THEN** those views are not required to run the volumetric-fog composite this slice

### Requirement: Camera-aligned froxel volume
Volumetric fog SHALL live in a camera-aligned 3D volume for the presenting view: XY cells of about **16 pixels** on the view rect, **64** Z slices, exponential view-depth with distribution scale **S = 32**. Slice range SHALL run from a small near offset through Fog view distance, not a separate world AABB. Density at each cell SHALL use exponential height falloff along world **Z** (up) relative to the active Fog entity’s world Z. The engine SHALL NOT run a separate 2D analytical height-fog pass this slice.

#### Scenario: Tile and slice counts
- **WHEN** the presenting view is 1920×1080 and Fog view distance is 60 meters
- **THEN** the volume XY size is ceil(view extent / 16) and Z size is 64

#### Scenario: Height uses world Z
- **WHEN** two froxels share XY and differ only in world Z
- **THEN** the higher-Z cell has lower or equal height density under positive falloff
- **AND** world Y is not the height axis

#### Scenario: No 2D height-fog pass
- **WHEN** volumetric fog is compositing on Viewport or Player
- **THEN** there is no additional fullscreen analytical height-fog pass

### Requirement: Unshadowed directional inject and thin temporal
Light scattering into the volume SHALL include the first Light enabled Directional whose contribution includes illumination, in stable EntityId order, with volumetric shadow factor 1. Point lights, spot lights, and area lights SHALL NOT inject this slice. The engine SHALL NOT sample shadow maps, virtual shadow pages, or extra volume-shadow rays for this fog. Temporal reprojection SHALL blend about **20% current / 80% history** on the pre-integrated scatter volume. The engine SHALL NOT apply a sticky `max(history, current)` highlight keep.

#### Scenario: Directional injects without shadows
- **WHEN** an active Fog and one illuminating Directional exist in the presenting scene
- **THEN** that Directional contributes in-scatter in the volume
- **AND** shadow maps do not darken froxels

#### Scenario: Locals do not inject
- **WHEN** the scene also has Point and Spot Lights
- **THEN** those lights do not add in-scatter in the volume this slice

#### Scenario: Thin temporal is on
- **WHEN** the presenting camera is still and Fog is active
- **THEN** successive frames blend about 20% of the current scatter volume with history

### Requirement: Stack on existing present path
Volumetric fog SHALL stack on the existing Viewport present path (editor Deferred lighting or Player Forward). The engine SHALL NOT add a GBuffer, deferred lighting path, or clustered light list in order to ship this fog. Volume resources SHALL stay off the Bindless color table.

#### Scenario: No new GBuffer
- **WHEN** volumetric fog is enabled
- **THEN** Forward opaque and transparent mesh draws still write the Player color and depth targets
- **AND** editor Deferred lighting still writes the imported present color and depth
- **AND** no additional GBuffer attachments are required for the fog composite
