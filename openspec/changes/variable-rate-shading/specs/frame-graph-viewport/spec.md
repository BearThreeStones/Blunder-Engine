## MODIFIED Requirements

### Requirement: Main viewport Scene records through Frame graph execute
When the editor viewport is forced Forward (`BLUNDER_EDITOR_DEFERRED=0`), that recording SHALL run as the Scene Pass callback of Frame graph `execute` on the viewport PRIMARY using the Forward Render Path. When the editor viewport or the Player uses the Deferred Render Path, the viewport graph SHALL NOT add that Forward Scene Pass. Copy SHALL remain the only Sink. `tickVulkan` SHALL NOT call Forward or Deferred `renderFrame` on a branch that bypasses that `execute`. PRIMARY begin, end, and submit SHALL remain the caller, not the graph and not the recorder.

#### Scenario: Default editor Scene uses execute
- **WHEN** the editor records a main viewport frame on the Deferred Render Path
- **THEN** G-buffer then lighting SHALL run inside Frame graph `execute`
- **AND** the graph SHALL NOT add a Forward Scene Pass
- **AND** `tickVulkan` SHALL NOT call Deferred `renderFrame` outside that execute

#### Scenario: Player Scene uses execute and stays Forward
- **WHEN** the Player records a main viewport frame
- **THEN** G-buffer then lighting SHALL run inside Frame graph `execute`
- **AND** the graph SHALL NOT add a Forward Scene Pass

### Requirement: Post-Scene overlay stages are Passes on the viewport graph
After the last shading Pass (Scene when the graph adds Scene; Lighting when Deferred), the same viewport graph SHALL add post-shading Passes in this order when built: Outline, Line+AA, SSAO, Screen overlays, then Copy. Live order among built Passes SHALL match that add order. `tickVulkan` SHALL NOT record those stages on a branch after `execute` returns. Outline ID, overlay line MRT, and SSAO extra images SHALL stay inside those callbacks and SHALL NOT be imported. Color-writing overlay Passes (Outline, Line+AA, SSAO when it writes color, Screen overlays) SHALL declare color as Read Sampled, then Write ColorAttachment, then Read Sampled on the same Pass. Line+AA SHALL be one Pass. When that Pass is built, it SHALL always declare that color handshake, including when overlay AA is off. SSAO’s first depth access SHALL be Sampled Read. Camera Preview, Mesh Preview Render, Scene Thumbnail Render, and Capture SHALL NOT record through that viewport graph. Camera Preview SHALL still record after that `execute`, using the Deferred Render Path into its dedicated offscreen (not Forward `renderFrameTo`).

#### Scenario: Default editor overlays run inside execute
- **WHEN** the editor records a main viewport frame that includes outline, overlay lines, overlay AA, SSAO, and screen overlays
- **THEN** those stages SHALL record as Passes of that Frame graph `execute`
- **AND** their relative order SHALL be Outline, then Line+AA, then SSAO, then Screen overlays, then Copy
- **AND** `tickVulkan` SHALL NOT record them after `execute` returns

#### Scenario: Camera Preview stays off the viewport graph
- **WHEN** Camera Preview is visible
- **THEN** that image SHALL be produced by Deferred recording into its dedicated offscreen
- **AND** it SHALL NOT be a Pass on the main viewport graph
- **AND** it SHALL still record after that viewport `execute`
- **AND** it SHALL NOT be produced by Forward `renderFrameTo`
