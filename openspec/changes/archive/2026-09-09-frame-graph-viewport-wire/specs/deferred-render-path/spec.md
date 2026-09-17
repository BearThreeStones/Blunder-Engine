## ADDED Requirements

### Requirement: Editor Deferred records through Frame graph execute
When the editor viewport uses the Deferred Render Path, that path’s frame recording SHALL run as the Scene Pass callback of Frame graph execute. `tickVulkan` SHALL NOT record Deferred `renderFrame` on a branch that bypasses that execute. G-buffer geometry and lighting SHALL remain inside the Deferred Render Path. Blend-transparent and scene overlays owned by that path SHALL remain inside that same callback. Overlay, SSAO, and copy that today run after the path SHALL remain after execute.

#### Scenario: Deferred is not a tickVulkan bypass
- **WHEN** `BLUNDER_EDITOR_DEFERRED` is `1`
- **AND** the editor records a main viewport frame
- **THEN** Deferred `renderFrame` SHALL run inside Frame graph execute
- **AND** lighting SHALL still write the existing offscreen color
- **AND** the G-buffer SHALL NOT be a Frame graph Transient this slice
