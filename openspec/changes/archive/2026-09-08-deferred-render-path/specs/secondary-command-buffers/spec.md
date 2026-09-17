## MODIFIED Requirements

### Requirement: Named visual passes execute secondary command buffers
When a Vulkan device exists, the engine SHALL record the following passes that run on that frame into SECONDARY command buffers, and the PRIMARY command buffer SHALL begin each of those passes with secondary contents and execute those buffers: shadow, forward color, deferred G-buffer, deferred lighting, selection outline, overlay lines, overlay line AA, SSAO, and screen overlays. GPU pick and Texture Loader copies SHALL remain PRIMARY. Deferred G-buffer and deferred lighting SHALL run only when the editor viewport uses the Deferred Render Path.

#### Scenario: Viewport frame uses secondaries for scene and overlays
- **WHEN** the editor records a viewport frame that includes shadow, forward, outline, overlay lines, SSAO, and screen overlays
- **THEN** those passes SHALL be recorded as SECONDARY command buffers
- **AND** the PRIMARY SHALL execute them inside the matching render passes
- **AND** GPU pick and texture upload command buffers SHALL still be PRIMARY

#### Scenario: Deferred viewport uses secondaries for G-buffer and lighting
- **WHEN** the editor viewport uses the Deferred Render Path
- **THEN** the G-buffer pass and the lighting pass SHALL be recorded as SECONDARY command buffers
- **AND** the PRIMARY SHALL execute them inside the matching render passes

### Requirement: Forward color keeps opaque, scene overlay, then transparent order
When a view uses the Forward Render Path, the forward color pass SHALL execute three SECONDARY command buffers in this order inside one subpass: opaque meshes, scene overlays, then transparent meshes. Pass order relative to shadow, outline, overlay lines, overlay AA, SSAO, and screen overlays SHALL stay as it is today for that Forward view.

#### Scenario: Blend meshes still composite over the grid
- **WHEN** a Forward Render Path frame draws opaque meshes, a depth-tested grid, and transparent meshes
- **THEN** opaque draws SHALL run first
- **AND** scene overlays SHALL run next
- **AND** transparent draws SHALL run last
- **AND** that order SHALL be the same as before this change

## ADDED Requirements

### Requirement: Deferred viewport keeps lighting then scene overlay then transparent
When the editor viewport uses the Deferred Render Path, the engine SHALL execute SECONDARY command buffers in this order after the shadow pass: G-buffer (opaque), lighting into offscreen color, scene overlays, then transparent meshes. Outline, overlay lines, overlay AA, SSAO, and screen overlays SHALL keep their existing order after that.

#### Scenario: Deferred blend meshes still composite over the grid
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** a frame draws opaque meshes, a depth-tested grid, and transparent meshes
- **THEN** G-buffer and lighting SHALL run first
- **AND** scene overlays SHALL run next
- **AND** transparent draws SHALL run last
