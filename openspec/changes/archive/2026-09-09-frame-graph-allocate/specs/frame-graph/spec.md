## ADDED Requirements

### Requirement: Frame graph allocate owns live Transients
After a successful compile whose Setup has not been mutated since, `allocate()` SHALL ask the caller-supplied Frame graph allocator for an owned RHI texture or buffer for each live Transient, using that resource’s Frame graph resource desc. DCE’d Transients SHALL NOT be allocated. External resources SHALL NOT be allocated. The graph SHALL take ownership of each created object. `allocate()` SHALL NOT compile, SHALL NOT run Execute callbacks, SHALL NOT alias, SHALL NOT insert barriers, SHALL NOT record commands, and SHALL NOT throw. Tests SHALL pass a stand-in allocator and SHALL NOT create a Vulkan device.

#### Scenario: Live Transient Texture is resolved after allocate
- **WHEN** a test creates a live Transient Texture with a legal desc, compiles, and allocates with a dummy allocator
- **THEN** allocate SHALL succeed
- **AND** `resolvedTexture` SHALL return the object the allocator created
- **AND** the resource SHALL remain Transient
- **AND** the test SHALL NOT call execute before that resolve
- **AND** the test SHALL NOT create a Vulkan device

#### Scenario: Live Transient Buffer pointer matches
- **WHEN** a test creates a live Transient Buffer with size 256, compiles, and allocates with a dummy allocator
- **THEN** `resolvedBuffer` SHALL return the object the allocator created
- **AND** `resourceDesc` size SHALL be 256

#### Scenario: DCE’d Transient is not created
- **WHEN** a test creates a Transient that compile DCE drops and a live Transient
- **AND** the test compiles and allocates
- **THEN** allocate SHALL succeed
- **AND** the allocator SHALL NOT be asked to create the DCE’d Transient
- **AND** `resolvedTexture` or `resolvedBuffer` on the DCE’d handle SHALL return null

#### Scenario: External is not allocated
- **WHEN** a test imports an External with a dummy non-null pointer and a live Transient
- **AND** the test compiles and allocates
- **THEN** the allocator SHALL NOT be asked to create the External
- **AND** `resolved*` on the External SHALL still return the imported pointer

#### Scenario: Sticky allocate does not create again
- **WHEN** a test compiles, allocates successfully, and calls `allocate()` again without mutating Setup
- **THEN** both calls SHALL succeed
- **AND** the allocator SHALL NOT create a second object for that Transient
- **AND** `resolved*` SHALL return the same pointer as the first allocate

### Requirement: Allocate returns failure without throwing
`allocate()` SHALL return `{ok, reason}` and SHALL NOT throw. If compile has never succeeded, the last compile was not `Ok`, or Setup changed since the last successful compile, `allocate()` SHALL return failure with reason `NotCompiled` and SHALL call the allocator for nothing. If any live Transient create returns null, `allocate()` SHALL return failure with reason `AllocFailed`, SHALL destroy every Transient object created in that call, and SHALL leave every Transient resolve null. `allocate()` SHALL NOT compile.

#### Scenario: Allocate before successful compile is NotCompiled
- **WHEN** a test calls `allocate()` before any successful compile, or after a failed compile
- **THEN** allocate SHALL return failure with reason `NotCompiled`
- **AND** allocate SHALL NOT throw
- **AND** the allocator SHALL NOT be asked to create
- **AND** allocate SHALL NOT compile

#### Scenario: AllocFailed rolls back
- **WHEN** a test compiles two live Transients
- **AND** the allocator returns a non-null object for the first and null for the second
- **THEN** allocate SHALL return failure with reason `AllocFailed`
- **AND** both Transient resolves SHALL be null
- **AND** execute SHALL return `NotAllocated`

### Requirement: Setup mutation destroys owned Transients
Any GraphBuilder Setup call after a successful allocate SHALL destroy graph-owned Transient RHI objects and SHALL NOT free External pointers. Transient resolve SHALL return null until compile and allocate succeed again.

#### Scenario: Setup after allocate clears Transient resolve
- **WHEN** a test compiles, allocates a live Transient, mutates Setup, and reads `resolvedTexture` or `resolvedBuffer` on that handle
- **THEN** that query SHALL return null
- **AND** execute SHALL return `NotCompiled`
- **AND** an External import pointer on the same graph SHALL still resolve if that handle was not discarded

## MODIFIED Requirements

### Requirement: Frame graph resolve reads attached RHI
`resolvedTexture` and `resolvedBuffer` SHALL return the RHI object on a Frame graph handle. For an External, that object SHALL be the non-owning import; that query SHALL be Setup state and SHALL NOT require compile. For a Transient, that object SHALL be the graph-owned result of a successful allocate; before allocate, and for a DCE’d Transient, the query SHALL return null. DCE SHALL NOT erase an External pointer. A never-created handle, a shape mismatch, or a null import SHALL return null and SHALL NOT throw. Resolve SHALL NOT allocate GPU memory. Tests SHALL use dummy RHI stand-ins and SHALL NOT create a Vulkan device.

#### Scenario: Imported Texture pointer is readable before compile
- **WHEN** a test imports an External Texture with a legal desc and a dummy RHI texture pointer
- **AND** the test reads `resolvedTexture` before compile
- **THEN** that query SHALL return the same pointer
- **AND** the resource SHALL remain External
- **AND** the test SHALL NOT create a Vulkan device

#### Scenario: Imported Buffer pointer matches
- **WHEN** a test imports an External Buffer with size 256 and a dummy RHI buffer pointer
- **THEN** `resolvedBuffer` SHALL return that pointer
- **AND** `resourceDesc` size SHALL be 256

#### Scenario: Miss cases return null
- **WHEN** a test queries `resolvedTexture` or `resolvedBuffer` on a never-created handle, a Transient that has not been allocated, a DCE’d Transient, or the wrong shape for that handle
- **THEN** the query SHALL return null
- **AND** the query SHALL NOT throw

#### Scenario: DCE does not erase an External pointer
- **WHEN** a test imports an External with a dummy non-null pointer
- **AND** compile drops that resource because no Sink can reach it
- **THEN** `resolvedTexture` or `resolvedBuffer` SHALL still return that pointer

### Requirement: Execute runs live Pass CPU callbacks
After a successful compile and a successful allocate whose Setup has not been mutated since, `execute()` SHALL run each live Pass’s callback in live Pass order. A live Pass with no callback SHALL be an empty run and SHALL remain in that order. A DCE’d Pass SHALL NOT run even if it has a callback. Callbacks SHALL be `void()` and SHALL NOT receive resolved Frame graph handles or RHI objects. `execute()` SHALL NOT compile, SHALL NOT allocate, SHALL NOT insert barriers, SHALL NOT record commands, and SHALL NOT run as a Job. Callbacks MUST NOT throw; `execute()` SHALL NOT catch. `execute()` SHALL NOT throw for graph-state failures. Tests SHALL still run without a Vulkan device.

#### Scenario: Live callbacks run in order and DCE does not run
- **WHEN** a test adds a Clear Pass and a Forward Sink that both Write the same Transient, plus a separate Pass that DCE drops
- **AND** the test records `setExecute` on all three Passes
- **AND** compile succeeds
- **AND** allocate succeeds
- **AND** the test calls `execute()`
- **THEN** execute SHALL succeed
- **AND** the Clear callback SHALL run before the Forward callback
- **AND** the DCE’d Pass callback SHALL NOT run

#### Scenario: Live Pass with no callback is an empty run
- **WHEN** a test compiles and allocates a live Sink subgraph where at least one live Pass has no `setExecute`
- **AND** other live Passes have callbacks
- **AND** the test calls `execute()`
- **THEN** execute SHALL succeed
- **AND** Passes that have callbacks SHALL still run
- **AND** the Pass with no callback SHALL NOT prevent execute from succeeding

#### Scenario: Same compile may execute twice
- **WHEN** a test compiles successfully, allocates successfully, and calls `execute()` twice without mutating Setup
- **THEN** both calls SHALL succeed
- **AND** each live callback SHALL run once per `execute()`

#### Scenario: Tests need no device for execute
- **WHEN** `frame_graph_test` runs execute cases
- **THEN** it SHALL execute graphs without creating a Vulkan device
- **AND** viewport present and forward mesh draws SHALL be unchanged

### Requirement: Execute returns NotCompiled without throwing
`execute()` SHALL return `{ok, reason}` and SHALL NOT throw. If compile has never succeeded on this graph, or the last compile was not `Ok`, or Setup changed since the last successful compile (`create`, `import`, `addPass`, `read`, `write`, `markSink`, or `setExecute`), `execute()` SHALL return failure with reason `NotCompiled` and SHALL run no callbacks. If compile is valid but allocate has not succeeded since that compile, `execute()` SHALL return failure with reason `NotAllocated` and SHALL run no callbacks. Those reasons SHALL NOT be compile failure reasons. `execute()` SHALL NOT compile. `execute()` SHALL NOT allocate.

#### Scenario: Execute before successful compile is NotCompiled
- **WHEN** a test calls `execute()` before any successful compile, or after a failed compile
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** execute SHALL NOT throw
- **AND** no callback SHALL run
- **AND** execute SHALL NOT compile
- **AND** the reason SHALL NOT be `InvalidPass`, `InvalidDesc`, `InvalidImport`, `DanglingAccess`, `NoSink`, or `Cycle`

#### Scenario: Execute after compile without allocate is NotAllocated
- **WHEN** a test compiles successfully and calls `execute()` without a successful allocate
- **THEN** execute SHALL return failure with reason `NotAllocated`
- **AND** no callback SHALL run
- **AND** execute SHALL NOT allocate

#### Scenario: Setup after compile dirties execute
- **WHEN** a test compiles successfully
- **AND** then mutates Setup
- **AND** then calls `execute()` without compiling again
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run

#### Scenario: Recompile after dirty allows execute
- **WHEN** a test compiles successfully, mutates Setup, compiles successfully again, allocates successfully, and calls `execute()`
- **THEN** execute SHALL succeed
- **AND** live callbacks from the latest compile SHALL run

#### Scenario: Execute after InvalidDesc is NotCompiled
- **WHEN** a test compiles a graph that fails with `InvalidDesc`
- **AND** the test calls `execute()`
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run

#### Scenario: Execute after InvalidImport is NotCompiled
- **WHEN** a test compiles a graph that fails with `InvalidImport`
- **AND** the test calls `execute()`
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run

#### Scenario: Execute after AllocFailed is NotAllocated
- **WHEN** a test compiles successfully
- **AND** allocate fails with `AllocFailed`
- **AND** the test calls `execute()`
- **THEN** execute SHALL return failure with reason `NotAllocated`
- **AND** no callback SHALL run
