## ADDED Requirements

### Requirement: Dummy Deferred depth handshake has no DepthAttachment→Sampled barrier before Lighting
Tests of this capability SHALL compile, allocate, plan, and execute a Dummy graph that models editor Deferred: External color and depth (and a shadow Read on Lighting), a G-buffer Pass that Writes depth as DepthAttachment then Reads Sampled and does not declare color, a Lighting Pass that Reads depth as Sampled, Reads shadow as Sampled, and Writes color as ColorAttachment then Reads Sampled, and a Copy Sink that Reads color as Sampled. Live Pass order SHALL be G-buffer then Lighting then Copy. Dummy SHALL NOT log a barrier whose `before` is the Lighting Pass, whose `from` usage is DepthAttachment, and whose `to` usage is Sampled. Copy SHALL be a Sink. The G-buffer Pass SHALL NOT be a Sink. Tests SHALL NOT create a Vulkan device and SHALL NOT include `vulkan.h` from `frame_graph.h`.

#### Scenario: Deferred Dummy live order and depth handshake
- **WHEN** that Dummy graph executes
- **THEN** live Pass order SHALL be G-buffer then Lighting then Copy
- **AND** Copy SHALL be a Sink
- **AND** the G-buffer Pass SHALL NOT be a Sink
- **AND** no Dummy barrier whose `before` is the Lighting Pass SHALL have DepthAttachment as the `from` usage and Sampled as the `to` usage
- **AND** the test SHALL NOT create a Vulkan device
