## MODIFIED Requirements

### Requirement: Editor viewport Deferred opt-in
The editor viewport SHALL use the Deferred Render Path unless `BLUNDER_EDITOR_DEFERRED` is a falsy value (`0`). When that variable is unset, the editor viewport SHALL use the Deferred Render Path. There SHALL NOT be a product settings UI for this opt-in. The Player SHALL use the Deferred Render Path and SHALL ignore `BLUNDER_EDITOR_DEFERRED` (that env remains an editor-viewport Forward escape hatch only).

#### Scenario: Default viewport is Forward
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is unset
- **THEN** the editor viewport uses the Deferred Render Path

#### Scenario: Env enables Deferred on the editor viewport
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **THEN** the editor viewport uses the Deferred Render Path for opaque mesh shading

#### Scenario: Player ignores the env
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `0`
- **AND** the Player presents a frame
- **THEN** the Player uses the Deferred Render Path

### Requirement: Other mesh surfaces stay Forward
Placement Preview SHALL use the Forward Render Path even when the editor viewport is Deferred. Camera Preview, Mesh Preview Render, Scene Thumbnail Render, and Capture SHALL use the Deferred Render Path. Those deferred previews SHALL NOT join the main viewport Frame graph as Passes.

#### Scenario: Camera Preview stays Forward
- **WHEN** Camera Preview is visible
- **THEN** the preview image is produced by the Deferred Render Path
- **AND** it is not a Pass on the main viewport graph

#### Scenario: Mesh Preview stays Forward
- **WHEN** Mesh Preview Render draws a Mesh Asset
- **THEN** that image is produced by the Deferred Render Path

### Requirement: Editor Deferred records through Frame graph execute
When the editor viewport uses the Deferred Render Path, Frame graph execute SHALL record a G-buffer Pass then a Lighting Pass. When the Player uses the Deferred Render Path, Frame graph execute SHALL record a G-buffer Pass then a Lighting Pass (no Editor Overlay chrome). `tickVulkan` SHALL NOT call Deferred `renderFrame`. G-buffer geometry SHALL remain inside the G-buffer Pass callback (after today’s Directional shadow pass on surfaces that run shadows). Lighting SHALL remain inside the Lighting Pass callback, followed by that path’s blend-transparent and (editor Viewport only) scene overlays (LOAD). Image-based VRS, when enabled, SHALL attach only inside that Lighting callback’s lighting render pass, then Sobel SHALL dispatch before LOAD / copy. Outline, overlay lines, overlay AA, SSAO, screen overlays, and copy SHALL record as Passes on the editor viewport graph after the Lighting Pass. Those stages SHALL NOT write the G-buffer. The G-buffer and the fragment shading rate image SHALL NOT be Frame graph Transients this slice.

#### Scenario: Deferred is not a tickVulkan bypass
- **WHEN** the editor records a main viewport frame on the Deferred Render Path
- **THEN** G-buffer then lighting SHALL run inside Frame graph execute as two Passes
- **AND** lighting SHALL still write the existing offscreen color
- **AND** the G-buffer SHALL NOT be a Frame graph Transient this slice
- **AND** outline, gizmos, and copy SHALL still run on that same viewport graph after Lighting
- **AND** those stages SHALL NOT write the G-buffer
- **AND** `tickVulkan` SHALL NOT call Deferred `renderFrame`
