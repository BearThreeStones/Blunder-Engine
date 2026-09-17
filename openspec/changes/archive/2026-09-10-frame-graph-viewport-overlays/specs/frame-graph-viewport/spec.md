## ADDED Requirements

### Requirement: Post-Scene overlay stages are Passes on the viewport graph
After the Scene Pass, the same viewport graph SHALL add post-Scene Passes in this order when built: Outline, Line+AA, SSAO, Screen overlays, then Copy. Live order among built Passes SHALL match that add order. `tickVulkan` SHALL NOT record those stages on a branch after `execute` returns. Outline ID, overlay line MRT, and SSAO extra images SHALL stay inside those callbacks and SHALL NOT be imported. Color-writing overlay Passes (Outline, Line+AA, SSAO when it writes color, Screen overlays) SHALL declare color as Read Sampled, then Write ColorAttachment, then Read Sampled on the same Pass. Line+AA SHALL be one Pass. When that Pass is built, it SHALL always declare that color handshake, including when overlay AA is off. SSAO’s first depth access SHALL be Sampled Read. Camera Preview, Mesh Preview Render, Scene Thumbnail Render, and Capture SHALL NOT record through that viewport graph. Camera Preview SHALL still record after that `execute`.

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

## MODIFIED Requirements

### Requirement: Main viewport Scene records through Frame graph execute
When the editor or the Player records the main viewport Scene into the offscreen color and depth, that recording SHALL run as the Scene Pass callback of Frame graph `execute` on the viewport PRIMARY. The Scene Pass SHALL NOT be a Sink. Copy SHALL be the only Sink on that graph. `tickVulkan` SHALL NOT call the Forward or Deferred path `renderFrame` on a branch that bypasses that `execute`. PRIMARY begin, end, and submit SHALL remain the caller, not the graph and not the recorder.

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

### Requirement: Viewport Scene imports offscreen and shadow as Externals
The viewport graph SHALL import the current offscreen color, offscreen depth, and Directional shadow map as External textures. Color desc format SHALL be `R8G8B8A8_UNORM`. Depth and shadow desc format SHALL be `D32_SFLOAT`. The Scene Pass SHALL Write color as ColorAttachment then Read color as Sampled, Write depth as DepthAttachment then Read depth as Sampled, and Read the shadow map as Sampled. Overlay and copy Passes SHALL NOT import additional images. This slice SHALL NOT create Transient resources for the viewport graph. Frame graph format SHALL NOT gain new values. Frame graph usage SHALL NOT gain new values.

#### Scenario: External-only viewport graph compiles
- **WHEN** the viewport graph imports those three Externals with legal descs and a non-null texture each
- **AND** the Scene Pass has those accesses and is not a Sink
- **AND** Copy is a Sink that Reads color as Sampled
- **THEN** compile SHALL succeed
- **AND** allocate SHALL create no Transient GPU objects

## REMOVED Requirements

### Requirement: Overlays stay after Scene execute
**Reason**: Post-Scene stages move onto the same Frame graph. Copy is the Sink. Camera Preview stays after `execute`.
**Migration**: Outline, Line+AA, SSAO, Screen overlays, and Copy record as Passes before `execute` returns. Camera Preview, Mesh Preview, Scene Thumbnail, and Capture stay off this graph.
