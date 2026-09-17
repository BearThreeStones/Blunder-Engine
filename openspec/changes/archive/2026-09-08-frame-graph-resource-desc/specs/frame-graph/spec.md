## MODIFIED Requirements

### Requirement: GraphBuilder records Setup
A caller SHALL record Frame graph Setup through GraphBuilder: create a Transient resource and import an External resource, each with a required Frame graph resource desc of the same type (shape remains a separate argument), add Passes that Read and Write Resources with a usage (Sampled, ColorAttachment, DepthAttachment, or Storage), mark Sink Passes, and record each Pass’s Execute callback with `setExecute` as `void()`. A Texture desc SHALL include Frame graph format, width, height, sample count, and mip count. A Buffer desc SHALL include size in bytes. The desc SHALL NOT include clear, initial layout, usage flags, array layers, 3D, cube, or host-visible. Frame graph format SHALL be `Undefined`, `R8G8B8A8_UNORM`, or `D32_SFLOAT`. A Buffer desc SHALL have no format. Setup SHALL NOT parse JSON, SHALL NOT use a Blackboard, and SHALL NOT allocate GPU memory. `read`, `write`, `markSink`, or `setExecute` on a Pass handle that was never added SHALL NOT throw; compile SHALL later return `InvalidPass`. Any of those Setup calls, including `create` and `import`, after a successful compile SHALL dirty that compile. `resourceDesc` SHALL return the stored desc without compile. DCE SHALL NOT erase that desc. A handle that was never created or imported SHALL return a default-constructed desc and SHALL NOT throw.

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

### Requirement: External resources are imported not created
An External resource SHALL be imported with a required Frame graph resource desc. Compile SHALL track Reads and Writes of it. The Frame graph SHALL NOT treat an External resource as a Transient it would allocate. Import SHALL NOT attach a device object.

#### Scenario: Imported viewport-color stand-in
- **WHEN** a test imports an External Texture with format `R8G8B8A8_UNORM`, width 8, height 8, sample count 1, mip count 1, adds a Sink Pass that Writes it as ColorAttachment, and compiles
- **THEN** compile SHALL succeed
- **AND** that Resource SHALL remain External
- **AND** compile SHALL NOT report it as a Transient the graph owns
- **AND** `resourceDesc` SHALL match that desc

### Requirement: Texture and Buffer share access rules
A Frame graph resource SHALL be a Texture or a Buffer. Create, import, Read, Write, compile edges, DCE, and Resource lifetime SHALL apply to both shapes. Usage SHALL NOT be a resource type. Attachment and Reference SHALL NOT be resource types. A Buffer desc SHALL record size in bytes; compile SHALL ignore Texture format and extent on a Buffer. A Texture desc SHALL ignore Buffer size.

#### Scenario: Buffer Transient write then read
- **WHEN** a test creates a Transient Buffer with size 256, adds a Pass that Writes it as Storage, adds a Sink Pass that Reads it as Storage, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the writer then the Sink
- **AND** that Buffer’s Resource lifetime SHALL cover both Passes
- **AND** `resourceDesc` size SHALL be 256

### Requirement: Compile returns failure without throwing
Compile SHALL NOT throw. When Setup or the live subgraph is invalid, compile SHALL return one failure and an empty live Pass order, in this order: `InvalidPass` (a Pass handle that was never added), then `InvalidDesc` (a created or imported Resource whose Frame graph resource desc is illegal), then `DanglingAccess` (a Resource that was never created or imported), then `NoSink`, then `Cycle` (a cycle among Passes a Sink can reach). A Texture desc SHALL be illegal when format is `Undefined`, or width or height is 0, or sample count is 0, or mip count is 0. A Buffer desc SHALL be illegal when size is 0. Create and import SHALL accept an illegal desc and SHALL NOT throw. An illegal desc SHALL fail compile even if no Pass uses that Resource and DCE would drop it. Unreachable Passes SHALL NOT be a compile failure; DCE SHALL drop them, including unreachable Passes that cycle among themselves.

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

#### Scenario: Illegal Texture desc fails compile
- **WHEN** a test creates a Transient Texture whose desc has format `Undefined`, or width or height 0, or sample count 0, or mip count 0
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidDesc`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Unused illegal desc fails compile
- **WHEN** a test creates a Resource with an illegal desc and adds a Sink subgraph that does not use that Resource
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidDesc`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Illegal Buffer size fails compile
- **WHEN** a test creates a Transient Buffer with size 0
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidDesc`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Failure reasons follow priority
- **WHEN** a test’s Setup is invalid in more than one of: never-added Pass handle, illegal desc, dangling Resource, no Sink, live cycle
- **AND** the test compiles
- **THEN** compile SHALL return exactly one of those reasons, in this preference: `InvalidPass`, then `InvalidDesc`, then `DanglingAccess`, then `NoSink`, then `Cycle`
- **AND** the live Pass order SHALL be empty

#### Scenario: InvalidPass beats InvalidDesc
- **WHEN** a test’s Setup has a never-added Pass handle and an illegal desc and a dangling access
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidPass`

#### Scenario: InvalidDesc beats DanglingAccess
- **WHEN** a test’s Setup has an illegal desc and a dangling access and no never-added Pass handle
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidDesc`

### Requirement: Execute returns NotCompiled without throwing
`execute()` SHALL return `{ok, reason}` and SHALL NOT throw. If compile has never succeeded on this graph, or the last compile was not `Ok`, or Setup changed since the last successful compile (`create`, `import`, `addPass`, `read`, `write`, `markSink`, or `setExecute`), `execute()` SHALL return failure with reason `NotCompiled` and SHALL run no callbacks. That reason SHALL NOT be a compile failure reason. `execute()` SHALL NOT compile.

#### Scenario: Execute before successful compile is NotCompiled
- **WHEN** a test calls `execute()` before any successful compile, or after a failed compile
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** execute SHALL NOT throw
- **AND** no callback SHALL run
- **AND** execute SHALL NOT compile
- **AND** the reason SHALL NOT be `InvalidPass`, `InvalidDesc`, `DanglingAccess`, `NoSink`, or `Cycle`

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

#### Scenario: Execute after InvalidDesc is NotCompiled
- **WHEN** a test compiles a graph that fails with `InvalidDesc`
- **AND** the test calls `execute()`
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run
