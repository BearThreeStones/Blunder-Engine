# deferred-render-path Specification

## Purpose

Give the editor viewport an opt-in Deferred Render Path: opaque geometry writes a G-buffer, then lighting runs in screen space into the existing offscreen color, while other mesh shading surfaces stay on the Forward Render Path. Editor viewport dispatch through Frame graph execute is a G-buffer Pass then a Lighting Pass; the G-buffer images stay path-owned.

## Requirements

### Requirement: Editor viewport Deferred opt-in
The editor viewport SHALL use the Deferred Render Path only when `BLUNDER_EDITOR_DEFERRED` is set to a truthy value. When that variable is unset or not truthy, the editor viewport SHALL use the Forward Render Path. There SHALL NOT be a product settings UI for this opt-in. The Player SHALL ignore `BLUNDER_EDITOR_DEFERRED` and SHALL use the Forward Render Path.

#### Scenario: Default viewport is Forward
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is unset
- **THEN** the editor viewport uses the Forward Render Path

#### Scenario: Env enables Deferred on the editor viewport
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **THEN** the editor viewport uses the Deferred Render Path for opaque mesh shading

#### Scenario: Player ignores the env
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the Player presents a frame
- **THEN** the Player uses the Forward Render Path

### Requirement: Other mesh surfaces stay Forward
Camera Preview, Mesh Preview Render, Scene Thumbnail Render, and Capture SHALL use the Forward Render Path even when `BLUNDER_EDITOR_DEFERRED` is set.

#### Scenario: Camera Preview stays Forward
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** Camera Preview is visible
- **THEN** the preview image is produced by the Forward Render Path

#### Scenario: Mesh Preview stays Forward
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** Mesh Preview Render draws a Mesh Asset
- **THEN** that image is produced by the Forward Render Path

### Requirement: G-buffer then lighting into existing offscreen
When the editor viewport uses the Deferred Render Path, the geometry pass SHALL write a G-buffer (albedo, world-space normal, metallic, roughness, AO, unlit, two-sided, and G-buffer receiver id) plus the existing viewport offscreen depth. World position SHALL be reconstructed from that depth. Lighting SHALL read the G-buffer and SHALL write the existing viewport offscreen color. The G-buffer SHALL NOT be the offscreen color, the pick ID target, or the outline ID buffer. Empty pixels with no opaque geometry SHALL show the viewport background.

#### Scenario: Lighting writes the same offscreen color
- **WHEN** the editor viewport uses the Deferred Render Path
- **THEN** lighting writes the viewport offscreen color
- **AND** SSAO, outline, pick, and gizmos still use that offscreen color and depth

#### Scenario: Empty pixel is viewport background
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** a pixel has no opaque geometry
- **THEN** that pixel is the viewport background color

### Requirement: Opaque including skinned write the G-buffer
Opaque mesh draws, including opaque skinned draws, SHALL write the G-buffer. Alpha-tested opaque draws SHALL clip in the geometry pass. Unlit opaque draws SHALL still write the G-buffer so lighting can output albedo. CPU vs GPU skinning contracts SHALL NOT change.

#### Scenario: Skinned opaque still shades
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** an opaque skinned MeshRenderer is visible
- **THEN** that mesh is lit from Light Components using the same Matrix Palette as Forward

#### Scenario: Alpha clip punches a hole
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** an opaque material uses alpha clip
- **THEN** discarded fragments do not write G-buffer or depth for those pixels

### Requirement: Blend-transparent after scene overlays
When the editor viewport uses the Deferred Render Path, blend-transparent draws SHALL use the Forward Render Path after lighting, into the same offscreen color, with depth test on and depth write off. Scene overlays SHALL run after lighting and before transparent draws so the grid depth-tests against opaque depth and transparent composites over the grid. Scene overlays SHALL NOT be drawn into the G-buffer.

#### Scenario: Transparent composites over the grid
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** a frame draws opaque meshes, a depth-tested grid, and transparent meshes
- **THEN** lighting writes opaque shading first
- **AND** scene overlays run next
- **AND** transparent draws run last with Forward lighting

### Requirement: Directional shadows sampled in lighting
When the editor viewport uses the Deferred Render Path, Light shadows SHALL still be the single Directional shadow map chosen by the existing Light shadows rule. Lighting SHALL sample that map with reconstructed world position using the existing PCF. Point, Spot, and Area SHALL NOT cast shadows. The existing editor shadow env SHALL still gate whether that shadow pass runs in the editor.

#### Scenario: Shadows env still gates the map
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** `BLUNDER_EDITOR_SHADOWS` is unset
- **THEN** the editor does not run the Directional shadow pass

#### Scenario: Linked receiver still gets the Directional shadow
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** the shadow-casting Directional affects a MeshRenderer under Light linking
- **AND** editor shadows are on
- **THEN** that receiver’s pixels are shadowed with the existing PCF

### Requirement: Editor Deferred records through Frame graph execute
When the editor viewport uses the Deferred Render Path, Frame graph execute SHALL record a G-buffer Pass then a Lighting Pass. `tickVulkan` SHALL NOT call Deferred `renderFrame`. G-buffer geometry SHALL remain inside the G-buffer Pass callback (after today’s Directional shadow pass). Lighting SHALL remain inside the Lighting Pass callback, followed by that path’s blend-transparent and scene overlays (LOAD). Outline, overlay lines, overlay AA, SSAO, screen overlays, and copy SHALL record as Passes on that same viewport graph after the Lighting Pass. Those stages SHALL NOT write the G-buffer. The G-buffer SHALL NOT be a Frame graph Transient this slice.

#### Scenario: Deferred is not a tickVulkan bypass
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the editor records a main viewport frame
- **THEN** G-buffer then lighting SHALL run inside Frame graph execute as two Passes
- **AND** lighting SHALL still write the existing offscreen color
- **AND** the G-buffer SHALL NOT be a Frame graph Transient this slice
- **AND** outline, gizmos, and copy SHALL still run on that same viewport graph after Lighting
- **AND** those stages SHALL NOT write the G-buffer
- **AND** `tickVulkan` SHALL NOT call Deferred `renderFrame`

### Requirement: GPU-driven static opaque writes the G-buffer
When the editor viewport uses the Deferred Render Path, static opaque and alpha-clip MeshRenderers submitted through GPU-driven rendering SHALL write the G-buffer (including alpha clip in geometry). Skinned opaque SHALL still write the G-buffer from the CPU draw list. GPU-driven rendering SHALL NOT write a visibility buffer instead of the G-buffer.

#### Scenario: GPU-driven atrium fills the G-buffer
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** a GPU-driven static opaque MeshRenderer is visible
- **THEN** that geometry writes the G-buffer
- **AND** lighting shades those pixels from that G-buffer

### Requirement: G-buffer receiver id is the MeshRenderer
The G-buffer receiver id SHALL identify the MeshRenderer that wrote the pixel, not a Meshlet. GPU-driven Meshlets SHALL inherit that MeshRenderer’s id. The GPU-driven MeshRenderer slot range SHALL be large enough for a Sponza-scale static scene and SHALL NOT be limited to the Forward mesh draw cap. Unlit and two-sided flags SHALL remain distinct from that id. Light linking SHALL still apply per MeshRenderer.

#### Scenario: Meshlets share the MeshRenderer id
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** two Meshlets of the same GPU-driven MeshRenderer write neighboring pixels
- **THEN** those pixels store the same G-buffer receiver id

#### Scenario: More than 256 static receivers
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** more than 256 GPU-driven static MeshRenderers write the G-buffer
- **THEN** those receivers keep distinct ids
- **AND** Light linking still applies per MeshRenderer
