## ADDED Requirements

### Requirement: GPU-driven static opaque writes the G-buffer
When the editor viewport uses the Deferred Render Path, static opaque and alpha-clip MeshRenderers submitted through GPU-driven rendering SHALL write the G-buffer (including alpha clip in geometry). Skinned opaque SHALL still write the G-buffer from the CPU draw list. GPU-driven rendering SHALL NOT write a visibility buffer instead of the G-buffer.

#### Scenario: GPU-driven atrium fills the G-buffer
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** a GPU-driven static opaque MeshRenderer is visible
- **THEN** that geometry writes the G-buffer
- **AND** lighting shades those pixels from that G-buffer

### Requirement: G-buffer receiver id is the MeshRenderer
The G-buffer receiver id SHALL identify the MeshRenderer that wrote the pixel, not a Meshlet. GPU-driven Meshlets SHALL inherit that MeshRenderer’s id. The GPU-driven MeshRenderer slot range SHALL be large enough for a Sponza-scale static scene and SHALL NOT be limited to the Forward mesh draw cap. Unlit and two-sided flags SHALL remain distinct from that id. Light linking SHALL still apply per MeshRenderer.

#### Scenario: Meshlets share the MeshRenderer id
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** two Meshlets of the same GPU-driven MeshRenderer write neighboring pixels
- **THEN** those pixels store the same G-buffer receiver id

#### Scenario: More than 256 static receivers
- **WHEN** the editor viewport uses the Deferred Render Path
- **AND** more than 256 GPU-driven static MeshRenderers write the G-buffer
- **THEN** those receivers keep distinct ids
- **AND** Light linking still applies per MeshRenderer
