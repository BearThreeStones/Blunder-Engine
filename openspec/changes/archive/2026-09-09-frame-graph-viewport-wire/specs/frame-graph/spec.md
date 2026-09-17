## ADDED Requirements

### Requirement: Vulkan recorder lives outside the graph headers
A Vulkan Frame graph recorder SHALL implement `IFrameGraphRecorder` in a compilation unit that is not `frame_graph.h` or `frame_graph.cpp`. That recorder MAY map Frame graph resource state onto `VkImageLayout` and MAY call `vkCmdPipelineBarrier`. Graph `execute` SHALL still not include `vulkan.h` and SHALL still not call `vkCmdPipelineBarrier`. Tests of this capability SHALL still pass a stand-in recorder and SHALL NOT create a Vulkan device.

#### Scenario: Graph tests stay Dummy
- **WHEN** `frame_graph_test` runs execute cases
- **THEN** it SHALL pass a Dummy recorder
- **AND** it SHALL NOT create a Vulkan device
- **AND** it SHALL NOT include `vulkan.h` from `frame_graph.h`

## MODIFIED Requirements

### Requirement: Execute plays planned barriers onto the recorder
After compile, allocate, and a successful `planBarriers()`, `execute` SHALL take a Frame graph recorder argument each call and SHALL NOT store it. For each live Pass in live order, execute SHALL call `pipelineBarrier` on that recorder for every planned row whose `before` is that Pass, in `barriers()` order, then SHALL run that Pass’s callback with the same recorder. A live Pass with no callback SHALL still receive its barriers. The recorder’s method in this slice SHALL be `pipelineBarrier` on a Frame graph barrier row. Execute SHALL NOT map Frame graph resource state onto `VkImageLayout`, SHALL NOT call `vkCmdPipelineBarrier`, and SHALL NOT begin, end, or submit a command buffer. A Vulkan recorder outside the graph headers MAY map those rows and MAY call `vkCmdPipelineBarrier`. Tests of this capability SHALL pass a stand-in recorder that logs those rows and SHALL NOT create a Vulkan device.

#### Scenario: Clear then Forward records first-use then WAW around callbacks
- **WHEN** a test creates a live Transient Texture, adds a Clear Pass that Writes it as ColorAttachment, adds a Forward Sink that Writes it as ColorAttachment, records `setExecute` on both, compiles, allocates, calls `planBarriers()`, and calls `execute` with a Dummy recorder
- **THEN** execute SHALL succeed
- **AND** the Dummy log SHALL contain the Transient first-use row, then the Clear callback, then the ColorAttachment WAW row, then the Forward callback
- **AND** the Forward callback SHALL receive the same Dummy recorder passed to `execute`
- **AND** the test SHALL NOT create a Vulkan device

#### Scenario: Empty External plan records only the callback
- **WHEN** a test imports an External Texture, adds a Sink that Writes it as ColorAttachment, records `setExecute` on that Sink, compiles, allocates, calls `planBarriers()` (empty list), and calls `execute` with a Dummy recorder
- **THEN** execute SHALL succeed
- **AND** the Dummy SHALL receive zero `pipelineBarrier` calls
- **AND** the Sink callback SHALL run once

#### Scenario: RAW is recorded before the reader; Read then Read is not
- **WHEN** a test Writes a live Transient as ColorAttachment then a later live Pass Reads it as Sampled
- **AND** two consecutive live Passes Read it as Sampled
- **AND** the test compiles, allocates, calls `planBarriers()`, and calls `execute` with a Dummy recorder
- **THEN** the Dummy log SHALL contain the RAW row before the first Sampled reader callback
- **AND** the Dummy log SHALL NOT contain a barrier between the two Sampled Reads

#### Scenario: Transient Buffer RAW is recorded before the reader
- **WHEN** a test creates a live Transient Buffer with size 256, a Pass that Writes it as Storage, a later live Pass that Reads it as Storage, compiles, allocates, calls `planBarriers()`, and calls `execute` with a Dummy recorder
- **THEN** the Dummy log SHALL contain that Buffer’s RAW row before the reader callback

### Requirement: Execute runs live Pass CPU callbacks
After a successful compile, a successful allocate, and a successful `planBarriers()` whose Setup has not been mutated since, `execute` SHALL take a Frame graph recorder and SHALL record each live Pass in live Pass order onto that recorder. A live Pass with no callback SHALL be an empty run after its barriers and SHALL remain in that order. A DCE’d Pass SHALL NOT run even if it has a callback. Callbacks SHALL be `void(IFrameGraphRecorder&)` and SHALL receive the same recorder `execute` was given. `execute` SHALL NOT compile, SHALL NOT allocate, SHALL NOT call `planBarriers()`, SHALL NOT map to `VkImageLayout`, SHALL NOT call `vkCmdPipelineBarrier`, SHALL NOT begin, end, or submit a command buffer, and SHALL NOT run as a Job. At the start of a successful execute, execute SHALL snapshot live Pass order, the barrier list, and callbacks so a forbidden Setup mutation does not UAF; remaining Passes SHALL still record from that copy and execute SHALL still return success. Callbacks MUST NOT throw and MUST NOT reenter; `execute` SHALL NOT catch. `execute` SHALL NOT throw for graph-state failures. Tests of this capability SHALL still run without a Vulkan device.

#### Scenario: Live callbacks run in order and DCE does not run
- **WHEN** a test adds a Clear Pass and a Forward Sink that both Write the same Transient, plus a separate Pass that DCE drops
- **AND** the test records `setExecute` on all three Passes
- **AND** compile succeeds
- **AND** allocate succeeds
- **AND** `planBarriers()` succeeds
- **AND** the test calls `execute` with a Dummy recorder
- **THEN** execute SHALL succeed
- **AND** the Clear callback SHALL run before the Forward callback
- **AND** the DCE’d Pass callback SHALL NOT run

#### Scenario: Live Pass with no callback is an empty run
- **WHEN** a test compiles, allocates, and plans a live Sink subgraph where at least one live Pass has no `setExecute`
- **AND** other live Passes have callbacks
- **AND** the test calls `execute` with a Dummy recorder
- **THEN** execute SHALL succeed
- **AND** Passes that have callbacks SHALL still run
- **AND** the Pass with no callback SHALL NOT prevent execute from succeeding
- **AND** that Pass’s planned barriers SHALL still be recorded

#### Scenario: Same compile may execute twice
- **WHEN** a test compiles successfully, allocates successfully, calls `planBarriers()` successfully, and calls `execute` with a Dummy recorder twice without mutating Setup
- **THEN** both calls SHALL succeed
- **AND** each live callback SHALL run once per `execute`
- **AND** the Dummy log sequence SHALL match on the second call

#### Scenario: Tests need no device for execute
- **WHEN** `frame_graph_test` runs execute cases
- **THEN** it SHALL execute graphs without creating a Vulkan device
- **AND** it SHALL use a Dummy recorder
