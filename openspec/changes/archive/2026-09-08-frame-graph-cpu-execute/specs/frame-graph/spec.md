## MODIFIED Requirements

### Requirement: GraphBuilder records Setup
A caller SHALL record Frame graph Setup through GraphBuilder: create a Transient resource, import an External resource, add Passes that Read and Write Resources with a usage (Sampled, ColorAttachment, DepthAttachment, or Storage), mark Sink Passes, and record each Pass’s Execute callback with `setExecute` as `void()`. Setup SHALL NOT parse JSON, SHALL NOT use a Blackboard, and SHALL NOT allocate GPU memory. `read`, `write`, `markSink`, or `setExecute` on a Pass handle that was never added SHALL NOT throw; compile SHALL later return `InvalidPass`. Any of those Setup calls, including `create` and `import`, after a successful compile SHALL dirty that compile.

#### Scenario: Two-Pass Transient then Sink
- **WHEN** a test creates a Transient Texture, adds a Pass that Writes it, adds a Sink Pass that Reads it as Sampled, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the writer then the Sink
- **AND** that Transient’s Resource lifetime SHALL cover both Passes

#### Scenario: Invalid setExecute fails compile
- **WHEN** a test calls `setExecute` with a Pass handle that was never added
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidPass`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

### Requirement: Compile returns failure without throwing
Compile SHALL NOT throw. When Setup or the live subgraph is invalid, compile SHALL return one failure and an empty live Pass order, in this order: `InvalidPass` (a Pass handle that was never added), then `DanglingAccess` (a Resource that was never created or imported), then `NoSink`, then `Cycle` (a cycle among Passes a Sink can reach). Unreachable Passes SHALL NOT be a compile failure; DCE SHALL drop them, including unreachable Passes that cycle among themselves.

#### Scenario: Setup-order ping-pong is not a cycle
- **WHEN** a test adds Pass A that Writes X and Reads Y, Pass B that Writes Y and Reads X, marks B a Sink, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be A then B

#### Scenario: Cycle fails compile
- **WHEN** compile’s live subgraph still has a producer/consumer cycle after Setup-order RAW, WAW, and WAR edges
- **AND** the test compiles
- **THEN** compile SHALL return failure
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Unreachable cycle is dropped not failed
- **WHEN** a test builds an acyclic Sink subgraph and a separate pair of Passes that cycle on Resources no Sink can reach
- **AND** the test compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL omit the cycling Passes

#### Scenario: Dangling access fails compile
- **WHEN** a test adds a Pass that Reads or Writes a Resource that was never created or imported
- **AND** the test compiles
- **THEN** compile SHALL return failure
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Zero Sinks fails compile
- **WHEN** a test adds Passes but marks no Sink
- **AND** the test compiles
- **THEN** compile SHALL return failure
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Invalid Pass handle fails compile
- **WHEN** a test calls `read`, `write`, `markSink`, or `setExecute` with a Pass handle that was never added
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidPass`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Failure reasons follow priority
- **WHEN** a test’s Setup is invalid in more than one of: never-added Pass handle, dangling Resource, no Sink, live cycle
- **AND** the test compiles
- **THEN** compile SHALL return exactly one of those reasons, in this preference: `InvalidPass`, then `DanglingAccess`, then `NoSink`, then `Cycle`
- **AND** the live Pass order SHALL be empty

## ADDED Requirements

### Requirement: Execute runs live Pass CPU callbacks
After a successful compile whose Setup has not been mutated since, `execute()` SHALL run each live Pass’s callback in live Pass order. A live Pass with no callback SHALL be an empty run and SHALL remain in that order. A DCE’d Pass SHALL NOT run even if it has a callback. Callbacks SHALL be `void()` and SHALL NOT receive resolved Frame graph handles or RHI objects. `execute()` SHALL NOT compile, SHALL NOT allocate GPU memory, SHALL NOT insert barriers, SHALL NOT record commands, and SHALL NOT run as a Job. Callbacks MUST NOT throw; `execute()` SHALL NOT catch. `execute()` SHALL NOT throw for graph-state failures. Tests SHALL still run without a Vulkan device.

#### Scenario: Live callbacks run in order and DCE does not run
- **WHEN** a test adds a Clear Pass and a Forward Sink that both Write the same Transient, plus a separate Pass that DCE drops
- **AND** the test records `setExecute` on all three Passes
- **AND** compile succeeds
- **AND** the test calls `execute()`
- **THEN** execute SHALL succeed
- **AND** the Clear callback SHALL run before the Forward callback
- **AND** the DCE’d Pass callback SHALL NOT run

#### Scenario: Live Pass with no callback is an empty run
- **WHEN** a test compiles a live Sink subgraph where at least one live Pass has no `setExecute`
- **AND** other live Passes have callbacks
- **AND** the test calls `execute()`
- **THEN** execute SHALL succeed
- **AND** Passes that have callbacks SHALL still run
- **AND** the Pass with no callback SHALL NOT prevent execute from succeeding

#### Scenario: Same compile may execute twice
- **WHEN** a test compiles successfully and calls `execute()` twice without mutating Setup
- **THEN** both calls SHALL succeed
- **AND** each live callback SHALL run once per `execute()`

#### Scenario: Tests need no device for execute
- **WHEN** `frame_graph_test` runs execute cases
- **THEN** it SHALL execute graphs without creating a Vulkan device
- **AND** viewport present and forward mesh draws SHALL be unchanged

### Requirement: Execute returns NotCompiled without throwing
`execute()` SHALL return `{ok, reason}` and SHALL NOT throw. If compile has never succeeded on this graph, or the last compile was not `Ok`, or Setup changed since the last successful compile (`create`, `import`, `addPass`, `read`, `write`, `markSink`, or `setExecute`), `execute()` SHALL return failure with reason `NotCompiled` and SHALL run no callbacks. That reason SHALL NOT be a compile failure reason. `execute()` SHALL NOT compile.

#### Scenario: Execute before successful compile is NotCompiled
- **WHEN** a test calls `execute()` before any successful compile, or after a failed compile
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** execute SHALL NOT throw
- **AND** no callback SHALL run
- **AND** execute SHALL NOT compile
- **AND** the reason SHALL NOT be `InvalidPass`, `DanglingAccess`, `NoSink`, or `Cycle`

#### Scenario: Setup after compile dirties execute
- **WHEN** a test compiles successfully
- **AND** then mutates Setup
- **AND** then calls `execute()` without compiling again
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run

#### Scenario: Recompile after dirty allows execute
- **WHEN** a test compiles successfully, mutates Setup, compiles successfully again, and calls `execute()`
- **THEN** execute SHALL succeed
- **AND** live callbacks from the latest compile SHALL run

## REMOVED Requirements

### Requirement: v1 does not execute the graph
**Reason**: This slice adds CPU Execute of live Pass callbacks.
**Migration**: Record `void()` callbacks with GraphBuilder `setExecute` and call `execute()` after a successful compile. Handles still do not resolve to RHI objects; ForwardRenderPath is unchanged.
