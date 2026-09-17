# frame-graph-viewport Specification

## Purpose

Schedule the main viewport shading (one Scene Pass, or editor Deferred G-buffer then Lighting) and post-shading overlay and copy stages through Frame graph execute and a Vulkan recorder. Camera Preview and other mesh surfaces stay on today’s callers.

## Requirements

### Requirement: Main viewport Scene records through Frame graph execute
When the editor with Deferred unset, or the Player, records the main viewport Scene into the offscreen color and depth, that recording SHALL run as the Scene Pass callback of Frame graph `execute` on the viewport PRIMARY. That Scene SHALL use the Forward Render Path. The Scene Pass SHALL NOT be a Sink. Copy SHALL be the only Sink on that graph. `tickVulkan` SHALL NOT call Forward `renderFrame` on a branch that bypasses that `execute`. PRIMARY begin, end, and submit SHALL remain the caller, not the graph and not the recorder.

#### Scenario: Default editor Scene uses execute
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is unset
- **AND** the editor records a main viewport frame
- **THEN** the Scene shading SHALL run inside Frame graph `execute`
- **AND** that Scene SHALL use the Forward Render Path
- **AND** the graph SHALL NOT add a G-buffer Pass or a Lighting Pass
- **AND** `tickVulkan` SHALL NOT call Forward `renderFrame` outside that execute

#### Scenario: Player Scene uses execute and stays Forward
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the Player records a main viewport frame
- **THEN** the Scene shading SHALL run inside Frame graph `execute`
- **AND** that Scene SHALL use the Forward Render Path
- **AND** the graph SHALL NOT add a G-buffer Pass or a Lighting Pass

### Requirement: Editor Deferred is a G-buffer Pass then a Lighting Pass
When the editor viewport uses the Deferred Render Path, the viewport graph SHALL NOT add a Scene Pass. It SHALL add a G-buffer Pass then a Lighting Pass on that same graph, every Deferred tick, including when there are no opaque draws. The G-buffer Pass callback SHALL record today’s Directional shadow pass then the G-buffer render pass. The Lighting Pass callback SHALL record today’s lighting render pass then the post-lighting LOAD (scene overlays then blend-transparent). `tickVulkan` SHALL NOT call Deferred `renderFrame`. The G-buffer images SHALL stay path-owned and SHALL NOT be imported or allocated as Frame graph resources. Overlay and copy Passes SHALL still follow the Lighting Pass and SHALL NOT write the G-buffer.

#### Scenario: Editor deferred is two Passes not one Scene
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the editor records a main viewport frame
- **THEN** the graph SHALL add a G-buffer Pass then a Lighting Pass
- **AND** the graph SHALL NOT add a Scene Pass
- **AND** opaque shading SHALL still be G-buffer then lighting into the existing offscreen color
- **AND** that work SHALL run inside Frame graph `execute`
- **AND** `tickVulkan` SHALL NOT call Deferred `renderFrame`

#### Scenario: Deferred with omitted overlays still builds G-buffer and Lighting
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the editor records a main viewport frame with no active outline, no active line overlays, and SSAO disabled
- **THEN** the graph SHALL still add the G-buffer Pass and the Lighting Pass
- **AND** the graph SHALL NOT add Outline, Line+AA, or SSAO Passes
- **AND** Copy SHALL still run as the Sink

### Requirement: Deferred Passes declare depth and color handshakes
The G-buffer Pass SHALL Write depth as DepthAttachment then Read depth as Sampled. It SHALL NOT declare color. It SHALL NOT declare the shadow map. The Lighting Pass SHALL Read depth as Sampled, Read the shadow map as Sampled, and Write color as ColorAttachment then Read color as Sampled. Neither Pass SHALL be a Sink.

#### Scenario: Deferred External-only graph compiles
- **WHEN** the viewport graph imports offscreen color, offscreen depth, and the Directional shadow map as Externals with legal descs and a non-null texture each
- **AND** the G-buffer Pass has those depth accesses and is not a Sink
- **AND** the Lighting Pass has those depth, shadow, and color accesses and is not a Sink
- **AND** Copy is a Sink that Reads color as Sampled
- **THEN** compile SHALL succeed
- **AND** allocate SHALL create no Transient GPU objects

### Requirement: Viewport Scene imports offscreen and shadow as Externals
The viewport graph SHALL import the current offscreen color, offscreen depth, and Directional shadow map as External textures. Color desc format SHALL be `R8G8B8A8_UNORM`. Depth and shadow desc format SHALL be `D32_SFLOAT`. When the graph adds a Scene Pass, that Pass SHALL Write color as ColorAttachment then Read color as Sampled, Write depth as DepthAttachment then Read depth as Sampled, and Read the shadow map as Sampled. Overlay and copy Passes SHALL NOT import additional images. This slice SHALL NOT create Transient resources for the viewport graph. Frame graph format SHALL NOT gain new values. Frame graph usage SHALL NOT gain new values.

#### Scenario: External-only viewport graph compiles
- **WHEN** the viewport graph imports those three Externals with legal descs and a non-null texture each
- **AND** the Scene Pass has those accesses and is not a Sink
- **AND** Copy is a Sink that Reads color as Sampled
- **THEN** compile SHALL succeed
- **AND** allocate SHALL create no Transient GPU objects

### Requirement: Post-Scene overlay stages are Passes on the viewport graph
After the last shading Pass (Scene when the graph adds Scene; Lighting when the editor uses Deferred), the same viewport graph SHALL add post-shading Passes in this order when built: Outline, Line+AA, SSAO, Screen overlays, then Copy. Live order among built Passes SHALL match that add order. `tickVulkan` SHALL NOT record those stages on a branch after `execute` returns. Outline ID, overlay line MRT, and SSAO extra images SHALL stay inside those callbacks and SHALL NOT be imported. Color-writing overlay Passes (Outline, Line+AA, SSAO when it writes color, Screen overlays) SHALL declare color as Read Sampled, then Write ColorAttachment, then Read Sampled on the same Pass. Line+AA SHALL be one Pass. When that Pass is built, it SHALL always declare that color handshake, including when overlay AA is off. SSAO’s first depth access SHALL be Sampled Read. Camera Preview, Mesh Preview Render, Scene Thumbnail Render, and Capture SHALL NOT record through that viewport graph. Camera Preview SHALL still record after that `execute`.

#### Scenario: Default editor overlays run inside execute
- **WHEN** the editor records a main viewport frame that includes outline, overlay lines, overlay AA, SSAO, and screen overlays
- **THEN** those stages SHALL record as Passes of that Frame graph `execute`
- **AND** their relative order SHALL be Outline, then Line+AA, then SSAO, then Screen overlays, then Copy
- **AND** `tickVulkan` SHALL NOT record them after `execute` returns

#### Scenario: Camera Preview stays off the viewport graph
- **WHEN** Camera Preview is visible
- **THEN** that image SHALL still be produced by Forward `renderFrameTo`
- **AND** it SHALL NOT be a Pass on the main viewport graph
- **AND** it SHALL still record after that viewport `execute`

### Requirement: Overlay Passes follow today’s tickVulkan conditionals
The viewport graph SHALL add Outline only when the overlay system reports an active outline. It SHALL add Line+AA only when there are active line overlays. It SHALL add SSAO only when SSAO is enabled. It SHALL add Screen overlays every frame when an OverlaySystem exists. It SHALL add Copy every frame this tick records the graph. The graph SHALL NOT add empty stand-in Passes for omitted stages.

#### Scenario: Omitted overlay Passes are not built
- **WHEN** the editor records a main viewport frame with no active outline, no active line overlays, and SSAO disabled
- **THEN** the graph SHALL NOT add Outline, Line+AA, or SSAO Passes
- **AND** Copy SHALL still run as the Sink

### Requirement: Copy is the only Sink
Copy SHALL be last among viewport graph Passes and SHALL be the only Pass marked Sink. Copy SHALL Read color as Sampled. The Copy callback SHALL be today’s zero-copy shader-read transition or the CPU copy from `TRANSFER_SRC` then shader-read. Graph Setup SHALL NOT declare a Transfer usage.

#### Scenario: Copy stays the Sink when overlays are omitted
- **WHEN** the viewport graph adds Scene and Copy and does not add Outline, Line+AA, or SSAO
- **THEN** compile SHALL succeed
- **AND** Copy SHALL be a Sink
- **AND** Scene SHALL NOT be a Sink

#### Scenario: Copy stays the Sink on a Deferred graph
- **WHEN** the viewport graph adds a G-buffer Pass, a Lighting Pass, and Copy, and does not add Scene
- **THEN** compile SHALL succeed
- **AND** Copy SHALL be a Sink
- **AND** the G-buffer Pass SHALL NOT be a Sink
- **AND** the Lighting Pass SHALL NOT be a Sink

### Requirement: Vulkan recorder is not the graph header
The object `tickVulkan` passes to `execute` SHALL be a Vulkan Frame graph recorder that implements `pipelineBarrier` by recording `vkCmdPipelineBarrier` on the bound PRIMARY. That type SHALL NOT be declared in `frame_graph.h`. The recorder SHALL NOT begin, end, or submit the command buffer, and SHALL NOT draw or begin a render pass.

#### Scenario: Graph header has no vulkan.h
- **WHEN** a translation unit includes `frame_graph.h` without including `vulkan.h`
- **THEN** that include SHALL compile
- **AND** `frame_graph_test` SHALL still pass a Dummy recorder and SHALL NOT create a Vulkan device
