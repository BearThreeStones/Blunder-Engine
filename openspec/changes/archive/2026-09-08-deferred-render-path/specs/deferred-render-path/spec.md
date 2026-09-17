## Purpose

Give the editor viewport an opt-in Deferred Render Path: opaque geometry writes a G-buffer, then lighting runs in screen space into the existing offscreen color, while other mesh shading surfaces stay on the Forward Render Path.

## ADDED Requirements

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
