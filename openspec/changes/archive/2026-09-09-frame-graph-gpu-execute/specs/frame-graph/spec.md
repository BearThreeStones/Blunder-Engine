## ADDED Requirements

### Requirement: Execute plays planned barriers onto the recorder
After compile, allocate, and a successful `planBarriers()`, `execute` SHALL take a Frame graph recorder argument each call and SHALL NOT store it. For each live Pass in live order, execute SHALL call `pipelineBarrier` on that recorder for every planned row whose `before` is that Pass, in `barriers()` order, then SHALL run that Pass’s callback with the same recorder. A live Pass with no callback SHALL still receive its barriers. The recorder’s method in this slice SHALL be `pipelineBarrier` on a Frame graph barrier row. Execute SHALL NOT map Frame graph resource state onto `VkImageLayout`, SHALL NOT call `vkCmdPipelineBarrier`, SHALL NOT begin, end, or submit a command buffer, and SHALL NOT replace the Forward Render Path. Tests SHALL pass a stand-in recorder that logs those rows and SHALL NOT create a Vulkan device.

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

## MODIFIED Requirements

### Requirement: GraphBuilder records Setup
A caller SHALL record Frame graph Setup through GraphBuilder: create a Transient resource with a shape and a required Frame graph resource desc of the same type used on import; import an External resource with that desc plus a non-owning RHI texture or buffer (shape follows the pointer type); add Passes that Read and Write Resources with a usage (Sampled, ColorAttachment, DepthAttachment, or Storage); mark Sink Passes; and record each Pass’s Execute callback with `setExecute` as `void(IFrameGraphRecorder&)`. A Texture desc SHALL include Frame graph format, width, height, sample count, and mip count. A Buffer desc SHALL include size in bytes. The desc SHALL NOT include clear, initial layout, usage flags, array layers, 3D, cube, or host-visible. Frame graph format SHALL be `Undefined`, `R8G8B8A8_UNORM`, or `D32_SFLOAT`. A Buffer desc SHALL have no format. Setup SHALL NOT parse JSON, SHALL NOT use a Blackboard, and SHALL NOT allocate GPU memory. `read`, `write`, `markSink`, or `setExecute` on a Pass handle that was never added SHALL NOT throw; compile SHALL later return `InvalidPass`. Any of those Setup calls, including `create` and `import`, after a successful compile SHALL dirty that compile, that allocate, and that barrier plan: graph-owned Transient RHI objects SHALL be destroyed; External pointers SHALL NOT be freed; `barriers()` SHALL be empty until compile and `planBarriers()` succeed again. `resourceDesc` SHALL return the stored desc without compile. DCE SHALL NOT erase that desc. A handle that was never created or imported SHALL return a default-constructed desc and SHALL NOT throw.

#### Scenario: Two-Pass Transient then Sink
- **WHEN** a test creates a Transient Texture with a legal desc, adds a Pass that Writes it, adds a Sink Pass that Reads it as Sampled, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the writer then the Sink
- **AND** that Transient’s Resource lifetime SHALL cover both Passes

#### Scenario: Transient Texture desc is readable before compile
- **WHEN** a test creates a Transient Texture with format `R8G8B8A8_UNORM`, width 8, height 8, sample count 1, mip count 1
- **AND** the test reads `resourceDesc` before compile
- **THEN** that desc SHALL match those fields
- **AND** the resource SHALL remain Transient

#### Scenario: Invalid setExecute fails compile
- **WHEN** a test calls `setExecute` with a Pass handle that was never added
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidPass`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

### Requirement: planBarriers returns failure without throwing
`planBarriers()` SHALL return `{ok, reason}` and SHALL NOT throw. If compile has never succeeded, the last compile was not `Ok`, or Setup changed since the last successful compile, `planBarriers()` SHALL return failure with reason `NotCompiled` and `barriers()` SHALL be empty. An empty plan after a successful compile SHALL be Ok. A second `planBarriers()` without Setup mutation SHALL succeed and SHALL NOT rebuild the list. `planBarriers()` SHALL NOT compile.

#### Scenario: Plan before successful compile is NotCompiled
- **WHEN** a test calls `planBarriers()` before any successful compile, or after a failed compile
- **THEN** `planBarriers()` SHALL return failure with reason `NotCompiled`
- **AND** `planBarriers()` SHALL NOT throw
- **AND** `barriers()` SHALL be empty
- **AND** `planBarriers()` SHALL NOT compile

#### Scenario: Sticky plan does not rebuild
- **WHEN** a test compiles, calls `planBarriers()` successfully, and calls `planBarriers()` again without mutating Setup
- **THEN** both calls SHALL succeed
- **AND** `barriers()` SHALL match the first call’s list

#### Scenario: Execute without a plan still succeeds
- **WHEN** a test compiles successfully, allocates successfully, and calls `execute` with a Dummy recorder without a successful `planBarriers()`
- **THEN** execute SHALL return failure with reason `NotPlanned`
- **AND** the Dummy SHALL receive no `pipelineBarrier` calls
- **AND** no callback SHALL run

### Requirement: Execute runs live Pass CPU callbacks
After a successful compile, a successful allocate, and a successful `planBarriers()` whose Setup has not been mutated since, `execute` SHALL take a Frame graph recorder and SHALL record each live Pass in live Pass order onto that recorder. A live Pass with no callback SHALL be an empty run after its barriers and SHALL remain in that order. A DCE’d Pass SHALL NOT run even if it has a callback. Callbacks SHALL be `void(IFrameGraphRecorder&)` and SHALL receive the same recorder `execute` was given. `execute` SHALL NOT compile, SHALL NOT allocate, SHALL NOT call `planBarriers()`, SHALL NOT map to `VkImageLayout`, SHALL NOT call `vkCmdPipelineBarrier`, SHALL NOT begin, end, or submit a command buffer, and SHALL NOT run as a Job. At the start of a successful execute, execute SHALL snapshot live Pass order, the barrier list, and callbacks so a forbidden Setup mutation does not UAF; remaining Passes SHALL still record from that copy and execute SHALL still return success. Callbacks MUST NOT throw and MUST NOT reenter; `execute` SHALL NOT catch. `execute` SHALL NOT throw for graph-state failures. Tests SHALL still run without a Vulkan device.

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
- **AND** viewport present and forward mesh draws SHALL be unchanged

### Requirement: Execute returns NotCompiled without throwing
`execute` SHALL return `{ok, reason}` and SHALL NOT throw. Checks SHALL run in this order: if compile has never succeeded on this graph, or the last compile was not `Ok`, or Setup changed since the last successful compile (`create`, `import`, `addPass`, `read`, `write`, `markSink`, or `setExecute`), `execute` SHALL return failure with reason `NotCompiled` and SHALL record nothing. Else if allocate has not succeeded since that compile, `execute` SHALL return failure with reason `NotAllocated` and SHALL record nothing. Else if `planBarriers()` has not succeeded since that compile, `execute` SHALL return failure with reason `NotPlanned` and SHALL record nothing. Those reasons SHALL NOT be compile failure reasons. `execute` SHALL NOT compile. `execute` SHALL NOT allocate. `execute` SHALL NOT call `planBarriers()`.

#### Scenario: Execute before successful compile is NotCompiled
- **WHEN** a test calls `execute` with a Dummy recorder before any successful compile, or after a failed compile
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** execute SHALL NOT throw
- **AND** no callback SHALL run
- **AND** the Dummy SHALL receive no `pipelineBarrier` calls
- **AND** execute SHALL NOT compile
- **AND** the reason SHALL NOT be `InvalidPass`, `InvalidDesc`, `InvalidImport`, `DanglingAccess`, `NoSink`, or `Cycle`

#### Scenario: Setup after compile dirties execute
- **WHEN** a test compiles successfully
- **AND** then mutates Setup
- **AND** then calls `execute` with a Dummy recorder without compiling again
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run
- **AND** the Dummy SHALL receive no `pipelineBarrier` calls

#### Scenario: Execute after compile without allocate is NotAllocated
- **WHEN** a test compiles successfully and calls `execute` with a Dummy recorder without a successful allocate
- **THEN** execute SHALL return failure with reason `NotAllocated`
- **AND** no callback SHALL run
- **AND** execute SHALL NOT allocate
- **AND** the reason SHALL NOT be `NotPlanned`

#### Scenario: Recompile after dirty allows execute
- **WHEN** a test compiles successfully, mutates Setup, compiles successfully again, allocates successfully, calls `planBarriers()` successfully, and calls `execute` with a Dummy recorder
- **THEN** execute SHALL succeed
- **AND** live callbacks from the latest compile SHALL run

#### Scenario: Execute after InvalidDesc is NotCompiled
- **WHEN** a test compiles a graph that fails with `InvalidDesc`
- **AND** the test calls `execute` with a Dummy recorder
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run

#### Scenario: Execute after InvalidImport is NotCompiled
- **WHEN** a test compiles a graph that fails with `InvalidImport`
- **AND** the test calls `execute` with a Dummy recorder
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run

#### Scenario: Execute after AllocFailed is NotAllocated
- **WHEN** a test compiles successfully
- **AND** allocate fails with `AllocFailed`
- **AND** the test calls `execute` with a Dummy recorder
- **THEN** execute SHALL return failure with reason `NotAllocated`
- **AND** no callback SHALL run
