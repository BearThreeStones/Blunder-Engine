## Purpose

Schedule the main viewport Scene through Frame graph execute and a Vulkan recorder, while overlays and other mesh surfaces stay on today’s callers.

## ADDED Requirements

### Requirement: Main viewport Scene records through Frame graph execute
When the editor or the Player records the main viewport Scene into the offscreen color and depth, that recording SHALL run as the Scene Pass callback of Frame graph `execute` on the viewport PRIMARY. The Scene Pass SHALL be a Sink. `tickVulkan` SHALL NOT call the Forward or Deferred path `renderFrame` on a branch that bypasses that `execute`. PRIMARY begin, end, and submit SHALL remain the caller, not the graph and not the recorder.

#### Scenario: Default editor Scene uses execute
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is unset
- **AND** the editor records a main viewport frame
- **THEN** the Scene shading SHALL run inside Frame graph `execute`
- **AND** that Scene SHALL use the Forward Render Path
- **AND** `tickVulkan` SHALL NOT call Forward `renderFrame` outside that execute

#### Scenario: Player Scene uses execute and stays Forward
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the Player records a main viewport frame
- **THEN** the Scene shading SHALL run inside Frame graph `execute`
- **AND** that Scene SHALL use the Forward Render Path

### Requirement: Deferred opt-in is the Scene Pass callback
When the editor viewport uses the Deferred Render Path, the Scene Pass callback SHALL be that path’s `renderFrame`. G-buffer and lighting SHALL remain inside that path. The G-buffer SHALL NOT be imported as Frame graph Resources this slice.

#### Scenario: Editor deferred still goes through execute
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the editor records a main viewport frame
- **THEN** opaque shading SHALL still be G-buffer then lighting into the existing offscreen color
- **AND** that work SHALL run inside Frame graph `execute`
- **AND** `tickVulkan` SHALL NOT call Deferred `renderFrame` outside that execute

### Requirement: Viewport Scene imports offscreen and shadow as Externals
The Scene Pass SHALL import the current offscreen color, offscreen depth, and Directional shadow map as External textures. Color desc format SHALL be `R8G8B8A8_UNORM`. Depth and shadow desc format SHALL be `D32_SFLOAT`. The Scene Pass SHALL Write color as ColorAttachment, Write depth as DepthAttachment, and Read the shadow map as Sampled. This slice SHALL NOT create Transient resources for the viewport Scene. Frame graph format SHALL NOT gain new values.

#### Scenario: External-only Scene compiles
- **WHEN** the viewport Scene graph imports those three Externals with legal descs and a non-null texture each
- **AND** the Scene Pass is a Sink with those accesses
- **THEN** compile SHALL succeed
- **AND** allocate SHALL create no Transient GPU objects

### Requirement: Overlays stay after Scene execute
After Scene `execute` returns, the same PRIMARY SHALL still record outline, overlay lines, overlay AA, SSAO when enabled, screen overlays, then copy or shader-read transition, in that order. Camera Preview, Mesh Preview Render, Scene Thumbnail Render, and Capture SHALL NOT record through that viewport graph.

#### Scenario: Post-Scene order is unchanged
- **WHEN** the editor records a main viewport frame that includes outline, overlay lines, overlay AA, SSAO, and screen overlays
- **THEN** those passes SHALL record after Scene `execute`
- **AND** they SHALL keep that relative order

#### Scenario: Camera Preview stays off the viewport graph
- **WHEN** Camera Preview is visible
- **THEN** that image SHALL still be produced by Forward `renderFrameTo`
- **AND** it SHALL NOT be a Pass on the main viewport graph

### Requirement: Vulkan recorder is not the graph header
The object `tickVulkan` passes to `execute` SHALL be a Vulkan Frame graph recorder that implements `pipelineBarrier` by recording `vkCmdPipelineBarrier` on the bound PRIMARY. That type SHALL NOT be declared in `frame_graph.h`. The recorder SHALL NOT begin, end, or submit the command buffer, and SHALL NOT draw or begin a render pass.

#### Scenario: Graph header has no vulkan.h
- **WHEN** a translation unit includes `frame_graph.h` without including `vulkan.h`
- **THEN** that include SHALL compile
- **AND** `frame_graph_test` SHALL still pass a Dummy recorder and SHALL NOT create a Vulkan device
