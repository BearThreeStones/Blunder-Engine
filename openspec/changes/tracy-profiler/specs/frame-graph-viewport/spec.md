## ADDED Requirements

### Requirement: Named viewport Passes write engine GPU timestamps
When a windowed Editor or windowed Player records the main viewport graph, each live named shading or post Pass used for the HUD table (`viewport.gbuffer`, `viewport.lighting`, or Forward `viewport.scene`; optional `viewport.ssao` / `viewport.volumetric_fog`; Sink `viewport.copy`) SHALL record engine GPU timestamps into the Frame timing ring. A few internal GPU timestamps MAY record inside those callbacks (shadow fill, GPU-driven cull, froxel fill, lighting fullscreen triangle). There SHALL NOT be a GPU timestamp per draw, per MeshRenderer, or per Spot light. Camera Preview, Mesh Preview, and Scene Thumbnail SHALL NOT be required to fill the HUD Pass table this change.

#### Scenario: Deferred editor timestamps G-buffer and lighting
- **WHEN** the editor records a Deferred main viewport frame
- **THEN** the Frame timing ring SHALL include GPU times for `viewport.gbuffer` and `viewport.lighting`

#### Scenario: Player Forward timestamps scene
- **WHEN** the Player records a Forward main viewport frame
- **THEN** the Frame timing ring SHALL include a GPU time for `viewport.scene`
- **AND** the graph SHALL still add that Forward Scene Pass (this change SHALL NOT convert Player to Deferred)

### Requirement: Tracy GPU collect is after execute on PRIMARY
When Tracy Client is compiled in, matching GPU zones MAY record on the command buffer that is recording a Pass (including SECONDARY). `TracyVkCollect` SHALL run after that viewport graph `execute` returns, before `vkQueueSubmit`, on the PRIMARY, and SHALL NOT run inside a Vulkan render pass. PRIMARY begin, end, and submit SHALL remain the caller.

#### Scenario: Collect is outside the render pass
- **WHEN** Tracy Client is compiled in and the viewport graph execute has returned
- **THEN** collect SHALL run on the PRIMARY before submit
- **AND** collect SHALL not run inside an open render pass
