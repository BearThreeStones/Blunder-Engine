## ADDED Requirements

### Requirement: Dummy overlay handshake has no ColorAttachment barrier before the overlay callback
Tests of this capability SHALL compile, allocate, plan, and execute a Dummy graph that models a color-writing overlay: an External color, a Scene Pass that Writes ColorAttachment then Reads Sampled, an overlay Pass that Reads Sampled then Writes ColorAttachment then Reads Sampled, and a Copy Sink that Reads Sampled. Dummy SHALL NOT log a barrier whose `before` is that overlay Pass and whose `to` usage is ColorAttachment. Tests SHALL NOT create a Vulkan device and SHALL NOT include `vulkan.h` from `frame_graph.h`.

#### Scenario: Handshake overlay is not preceded by ColorAttachment
- **WHEN** that Dummy graph executes
- **THEN** the overlay callback SHALL run
- **AND** no Dummy barrier before that callback SHALL have ColorAttachment as the `to` usage
- **AND** Copy SHALL be a Sink
- **AND** Scene SHALL NOT be a Sink

### Requirement: Dummy omitted overlay Pass is not live
When a Dummy viewport-shaped graph adds a Scene Pass and a Copy Sink and does not add an overlay Pass, compile live order SHALL be Scene then Copy. When a Dummy graph adds an extra Pass that no Sink can reach, that Pass SHALL NOT be live and its callback SHALL NOT run.

#### Scenario: Omitted overlay is not in live order
- **WHEN** a Dummy graph imports an External color, adds a Scene Pass that Writes ColorAttachment then Reads Sampled, does not add an overlay Pass, adds a Copy Sink that Reads Sampled, and compiles
- **THEN** live Pass order SHALL be Scene then Copy
- **AND** Copy SHALL be a Sink

#### Scenario: Unreachable overlay callback does not run
- **WHEN** a Dummy graph adds a Scene Pass, a Copy Sink, and a separate overlay Pass that no Sink can reach
- **AND** the test records `setExecute` on all three
- **AND** compile, allocate, `planBarriers()`, and Dummy `execute` succeed
- **THEN** the unreachable overlay callback SHALL NOT run
