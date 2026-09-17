## MODIFIED Requirements

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
