## ADDED Requirements

### Requirement: Frame graph barrier plan records live transitions
After a successful compile whose Setup has not been mutated since, `planBarriers()` SHALL record Frame graph barriers from live Pass order and Resource access. `planBarriers()` SHALL NOT require allocate, SHALL NOT compile, SHALL NOT run Execute callbacks, SHALL NOT alias, SHALL NOT record GPU commands, and SHALL NOT throw. Texture and Buffer SHALL use the same emit rules. Tests SHALL NOT create a Vulkan device.

Each recorded barrier SHALL name one live handle, a from Frame graph resource state, a to Frame graph resource state, the Pass just finished (`after`), and the next live Pass that accesses that handle (`before`). Frame graph resource state SHALL be AccessKind plus Usage, or Undefined before a Transient’s first live access. Undefined SHALL NOT be Frame graph format `Undefined`. `after` SHALL be invalid on a Transient’s first-use row. `barriers()` SHALL return those rows in `before` live Pass order, then handle index. Before a successful plan, `barriers()` SHALL be empty and SHALL NOT throw.

Emit rules: same-Usage Read→Read across Passes SHALL NOT emit a row. Same-Pass accesses SHALL NOT emit a row. Cross-Pass RAW, WAR, and WAW SHALL emit a row even when Usage is unchanged. Per `(Pass, handle)`, the leaving state SHALL be that Pass’s last access of the handle (Setup call order) and the entering state SHALL be the next accessing live Pass’s first access. A live Transient SHALL emit Undefined → its first live Pass entering state. An External SHALL NOT emit an incoming Undefined row. A DCE’d resource SHALL emit no rows.

#### Scenario: Clear then Forward is a ColorAttachment WAW
- **WHEN** a test creates a live Transient Texture, adds a Clear Pass that Writes it as ColorAttachment, adds a Forward Sink that Writes it as ColorAttachment, compiles, and calls `planBarriers()` without allocate and without execute
- **THEN** `planBarriers()` SHALL succeed
- **AND** `barriers()` SHALL include a row whose from and to are both Write plus ColorAttachment
- **AND** that row’s `after` SHALL be the Clear Pass
- **AND** that row’s `before` SHALL be the Forward Pass
- **AND** the test SHALL NOT create a Vulkan device

#### Scenario: Transient first use is Undefined; External is not
- **WHEN** a test creates a live Transient Texture and imports an External dummy Texture
- **AND** live Passes access both
- **AND** the test also creates a Transient that compile DCE drops
- **AND** the test compiles and calls `planBarriers()`
- **THEN** `barriers()` SHALL include a Transient first-use row from Undefined to that Transient’s first live Pass entering state
- **AND** that first-use row’s `after` SHALL be invalid
- **AND** `barriers()` SHALL NOT include an Undefined-from row for the External
- **AND** `barriers()` SHALL NOT include a row for the DCE’d Transient

#### Scenario: ColorAttachment Write then Sampled Read is RAW; Read then Read is not
- **WHEN** a test Writes a live Transient as ColorAttachment then a later live Pass Reads it as Sampled
- **AND** two consecutive live Passes Read it as Sampled
- **AND** the test compiles and calls `planBarriers()`
- **THEN** `barriers()` SHALL include a RAW row from Write plus ColorAttachment to Read plus Sampled
- **AND** `barriers()` SHALL NOT include a row between the two Sampled Reads

#### Scenario: Transient Buffer uses the same rules
- **WHEN** a test creates a live Transient Buffer with size 256, a Pass that Writes it as Storage, a later live Pass that Reads it as Storage, compiles, and calls `planBarriers()`
- **THEN** `planBarriers()` SHALL succeed
- **AND** `barriers()` SHALL include a RAW row for that Buffer
- **AND** `resourceDesc` size SHALL be 256

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
- **WHEN** a test compiles successfully, allocates successfully, and calls `execute()` without a successful `planBarriers()`
- **THEN** execute SHALL succeed
- **AND** live callbacks SHALL run

## MODIFIED Requirements

### Requirement: GraphBuilder records Setup
A caller SHALL record Frame graph Setup through GraphBuilder: create a Transient resource with a shape and a required Frame graph resource desc of the same type used on import; import an External resource with that desc plus a non-owning RHI texture or buffer (shape follows the pointer type); add Passes that Read and Write Resources with a usage (Sampled, ColorAttachment, DepthAttachment, or Storage); mark Sink Passes; and record each Pass’s Execute callback with `setExecute` as `void()`. A Texture desc SHALL include Frame graph format, width, height, sample count, and mip count. A Buffer desc SHALL include size in bytes. The desc SHALL NOT include clear, initial layout, usage flags, array layers, 3D, cube, or host-visible. Frame graph format SHALL be `Undefined`, `R8G8B8A8_UNORM`, or `D32_SFLOAT`. A Buffer desc SHALL have no format. Setup SHALL NOT parse JSON, SHALL NOT use a Blackboard, and SHALL NOT allocate GPU memory. `read`, `write`, `markSink`, or `setExecute` on a Pass handle that was never added SHALL NOT throw; compile SHALL later return `InvalidPass`. Any of those Setup calls, including `create` and `import`, after a successful compile SHALL dirty that compile, that allocate, and that barrier plan: graph-owned Transient RHI objects SHALL be destroyed; External pointers SHALL NOT be freed; `barriers()` SHALL be empty until compile and `planBarriers()` succeed again. `resourceDesc` SHALL return the stored desc without compile. DCE SHALL NOT erase that desc. A handle that was never created or imported SHALL return a default-constructed desc and SHALL NOT throw.

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

### Requirement: Setup mutation destroys owned Transients
Any GraphBuilder Setup call after a successful allocate SHALL destroy graph-owned Transient RHI objects and SHALL NOT free External pointers. Transient resolve SHALL return null until compile and allocate succeed again. Any GraphBuilder Setup call after a successful `planBarriers()` SHALL clear the barrier plan; `barriers()` SHALL be empty until compile and `planBarriers()` succeed again.

#### Scenario: Setup after allocate clears Transient resolve
- **WHEN** a test compiles, allocates a live Transient, mutates Setup, and reads `resolvedTexture` or `resolvedBuffer` on that handle
- **THEN** that query SHALL return null
- **AND** execute SHALL return `NotCompiled`
- **AND** an External import pointer on the same graph SHALL still resolve if that handle was not discarded

#### Scenario: Setup after planBarriers clears the list
- **WHEN** a test compiles, calls `planBarriers()` successfully, mutates Setup, and reads `barriers()`
- **THEN** `barriers()` SHALL be empty
- **AND** compile and `planBarriers()` again SHALL restore a non-empty table for that same live subgraph

### Requirement: Execute runs live Pass CPU callbacks
After a successful compile and a successful allocate whose Setup has not been mutated since, `execute()` SHALL run each live Pass’s callback in live Pass order. A live Pass with no callback SHALL be an empty run and SHALL remain in that order. A DCE’d Pass SHALL NOT run even if it has a callback. Callbacks SHALL be `void()` and SHALL NOT receive resolved Frame graph handles or RHI objects. `execute()` SHALL NOT compile, SHALL NOT allocate, SHALL NOT require `planBarriers()`, SHALL NOT insert barriers, SHALL NOT record commands, and SHALL NOT run as a Job. Callbacks MUST NOT throw; `execute()` SHALL NOT catch. `execute()` SHALL NOT throw for graph-state failures. Tests SHALL still run without a Vulkan device.

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
`execute()` SHALL return `{ok, reason}` and SHALL NOT throw. If compile has never succeeded on this graph, or the last compile was not `Ok`, or Setup changed since the last successful compile (`create`, `import`, `addPass`, `read`, `write`, `markSink`, or `setExecute`), `execute()` SHALL return failure with reason `NotCompiled` and SHALL run no callbacks. If compile is valid but allocate has not succeeded since that compile, `execute()` SHALL return failure with reason `NotAllocated` and SHALL run no callbacks. Missing `planBarriers()` SHALL NOT be an execute failure reason. Those reasons SHALL NOT be compile failure reasons. `execute()` SHALL NOT compile. `execute()` SHALL NOT allocate.

#### Scenario: Execute before successful compile is NotCompiled
- **WHEN** a test calls `execute()` before any successful compile, or after a failed compile
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** execute SHALL NOT throw
- **AND** no callback SHALL run
- **AND** execute SHALL NOT compile
- **AND** the reason SHALL NOT be `InvalidPass`, `InvalidDesc`, `InvalidImport`, `DanglingAccess`, `NoSink`, or `Cycle`

#### Scenario: Setup after compile dirties execute
- **WHEN** a test compiles successfully
- **AND** then mutates Setup
- **AND** then calls `execute()` without compiling again
- **THEN** execute SHALL return failure with reason `NotCompiled`
- **AND** no callback SHALL run

#### Scenario: Execute after compile without allocate is NotAllocated
- **WHEN** a test compiles successfully and calls `execute()` without a successful allocate
- **THEN** execute SHALL return failure with reason `NotAllocated`
- **AND** no callback SHALL run
- **AND** execute SHALL NOT allocate

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
