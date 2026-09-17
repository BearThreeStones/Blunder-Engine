# frame-graph Specification

## Purpose
CPU Frame graph Setup, compile, allocate of live Transients, CPU barrier plan, and GPU Execute for one frame of GPU work: Passes, Transient and External Resources, dead-Pass elimination from Sinks, Resource lifetimes, graph-owned Transient RHI objects after allocate, a CPU list of Frame graph barriers after `planBarriers()`, and live-order recording onto a Frame graph recorder. Compile SHALL NOT allocate GPU memory. Execute SHALL NOT call `vkCmdPipelineBarrier`. A Vulkan Frame graph recorder outside the graph headers MAY. Tests of this capability SHALL pass a stand-in allocator and a stand-in recorder and SHALL NOT create a Vulkan device. Viewport Scene, editor Deferred G-buffer/lighting, and overlay dispatch through that recorder is a separate capability.

## Requirements

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

### Requirement: Compile builds edges, order, DCE, and lifetimes
Frame graph compile SHALL derive producer/consumer edges from Resource access, produce a topological live Pass order, eliminate Passes that no Sink can reach, and record a Resource lifetime `[first access, last access]` on each live Resource. On one handle, edges SHALL follow Setup order (Pass add order, then that Pass’s read/write call order): RAW (Write → later Read), WAW (Write → later Write), and WAR (Read → later Write). Usage SHALL NOT grow extra edges. Same-Pass accesses SHALL NOT add an edge. Across Passes, a Read SHALL take an edge from the previous Write; a Write SHALL take an edge from the previous Write and from every Read after that previous Write. Read→Read SHALL NOT add an edge. Compile SHALL NOT introduce a Resource version type. Compile SHALL NOT allocate GPU memory, alias heaps, insert barriers, or record commands. Compile SHALL NOT run as a Job.

#### Scenario: Unreachable post-process is dropped
- **WHEN** a test adds a Sink subgraph and a separate post-process Pass that Writes a Transient nobody Reads toward a Sink
- **AND** the test compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL omit that post-process Pass
- **AND** that Transient SHALL NOT be live

#### Scenario: ColorAttachment write chain stays live
- **WHEN** a test creates a Transient Texture, adds a Pass that Writes it as ColorAttachment, adds a Sink Pass that Writes it as ColorAttachment, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the first writer then the Sink
- **AND** that Transient’s Resource lifetime SHALL cover both Passes

#### Scenario: Read then later Write keeps the reader
- **WHEN** a test adds a Pass that Writes a resource, a Pass that Reads it, a Sink Pass that Writes it, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL place the reader before the Sink writer

#### Scenario: Two readers before a Write stay unordered
- **WHEN** a test adds two Passes that Read a resource and a Sink Pass that Writes it, and compiles
- **THEN** compile SHALL succeed
- **AND** both reader Passes SHALL be live and before the Sink
- **AND** compile SHALL NOT require a unique order between the two readers

#### Scenario: Same-Pass read and write is not a cycle
- **WHEN** a test adds a Sink Pass that Writes then Reads the same handle, or Writes it twice, and compiles
- **THEN** compile SHALL succeed
- **AND** compile SHALL NOT return `Cycle`

### Requirement: External resources are imported not created
An External resource SHALL be imported with a required Frame graph resource desc and a non-owning RHI texture or buffer pointer. Compile SHALL track Reads and Writes of it. The Frame graph SHALL NOT treat an External resource as a Transient it would allocate. Import SHALL NOT take ownership of GPU memory. Import SHALL accept a null pointer and SHALL NOT throw.

#### Scenario: Imported viewport-color stand-in
- **WHEN** a test imports an External Texture with format `R8G8B8A8_UNORM`, width 8, height 8, sample count 1, mip count 1, and a dummy non-null RHI texture pointer, adds a Sink Pass that Writes it as ColorAttachment, and compiles
- **THEN** compile SHALL succeed
- **AND** that Resource SHALL remain External
- **AND** compile SHALL NOT report it as a Transient the graph owns
- **AND** `resourceDesc` SHALL match that desc
- **AND** `resolvedTexture` SHALL return that dummy pointer

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
- **WHEN** a test compiles successfully, allocates successfully, and calls `execute` with a Dummy recorder without a successful `planBarriers()`
- **THEN** execute SHALL return failure with reason `NotPlanned`
- **AND** the Dummy SHALL receive no `pipelineBarrier` calls
- **AND** no callback SHALL run

### Requirement: Execute plays planned barriers onto the recorder
After compile, allocate, and a successful `planBarriers()`, `execute` SHALL take a Frame graph recorder argument each call and SHALL NOT store it. For each live Pass in live order, execute SHALL call `pipelineBarrier` on that recorder for every planned row whose `before` is that Pass, in `barriers()` order, then SHALL run that Pass’s callback with the same recorder. A live Pass with no callback SHALL still receive its barriers. The recorder’s method in this slice SHALL be `pipelineBarrier` on a Frame graph barrier row. Execute SHALL NOT map Frame graph resource state onto `VkImageLayout`, SHALL NOT call `vkCmdPipelineBarrier`, SHALL NOT begin, end, or submit a command buffer, and SHALL NOT replace the Forward Render Path. A Vulkan recorder outside the graph headers MAY map those rows and MAY call `vkCmdPipelineBarrier`. Tests SHALL pass a stand-in recorder that logs those rows and SHALL NOT create a Vulkan device.

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

### Requirement: Texture and Buffer share access rules
A Frame graph resource SHALL be a Texture or a Buffer. Create, import, Read, Write, compile edges, DCE, and Resource lifetime SHALL apply to both shapes. Usage SHALL NOT be a resource type. Attachment and Reference SHALL NOT be resource types. A Buffer desc SHALL record size in bytes; compile SHALL ignore Texture format and extent on a Buffer. A Texture desc SHALL ignore Buffer size.

#### Scenario: Buffer Transient write then read
- **WHEN** a test creates a Transient Buffer with size 256, adds a Pass that Writes it as Storage, adds a Sink Pass that Reads it as Storage, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the writer then the Sink
- **AND** that Buffer’s Resource lifetime SHALL cover both Passes
- **AND** `resourceDesc` size SHALL be 256

### Requirement: Compile returns failure without throwing
Compile SHALL NOT throw. When Setup or the live subgraph is invalid, compile SHALL return one failure and an empty live Pass order, in this order: `InvalidPass` (a Pass handle that was never added), then `InvalidDesc` (a created or imported Resource whose Frame graph resource desc is illegal), then `InvalidImport` (an External resource imported with a null RHI pointer), then `DanglingAccess` (a Resource that was never created or imported), then `NoSink`, then `Cycle` (a cycle among Passes a Sink can reach). A Texture desc SHALL be illegal when format is `Undefined`, or width or height is 0, or sample count is 0, or mip count is 0. A Buffer desc SHALL be illegal when size is 0. Create and import SHALL accept an illegal desc and SHALL NOT throw. An illegal desc SHALL fail compile even if no Pass uses that Resource and DCE would drop it. A null import SHALL fail compile even if no Pass uses that Resource and DCE would drop it. Unreachable Passes SHALL NOT be a compile failure; DCE SHALL drop them, including unreachable Passes that cycle among themselves.

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

#### Scenario: Null import fails compile
- **WHEN** a test imports an External with a legal desc and a null RHI pointer
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidImport`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty
- **AND** `resolvedTexture` or `resolvedBuffer` SHALL return null

#### Scenario: Unused null import fails compile
- **WHEN** a test imports an External with a null RHI pointer and adds a Sink subgraph that does not use that Resource
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidImport`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Failure reasons follow priority
- **WHEN** a test’s Setup is invalid in more than one of: never-added Pass handle, illegal desc, null import, dangling Resource, no Sink, live cycle
- **AND** the test compiles
- **THEN** compile SHALL return exactly one of those reasons, in this preference: `InvalidPass`, then `InvalidDesc`, then `InvalidImport`, then `DanglingAccess`, then `NoSink`, then `Cycle`
- **AND** the live Pass order SHALL be empty

#### Scenario: InvalidPass beats InvalidDesc
- **WHEN** a test’s Setup has a never-added Pass handle and an illegal desc and a null import and a dangling access
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidPass`

#### Scenario: InvalidDesc beats DanglingAccess
- **WHEN** a test’s Setup has an illegal desc and a dangling access and no never-added Pass handle
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidDesc`

#### Scenario: InvalidDesc beats InvalidImport
- **WHEN** a test’s Setup has an illegal desc and a null import and a dangling access and no never-added Pass handle
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidDesc`

#### Scenario: InvalidImport beats DanglingAccess
- **WHEN** a test’s Setup has a null import and a dangling access and a legal desc and no never-added Pass handle
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidImport`

### Requirement: Execute runs live Pass CPU callbacks
After a successful compile, a successful allocate, and a successful `planBarriers()` whose Setup has not been mutated since, `execute` SHALL take a Frame graph recorder and SHALL record each live Pass in live Pass order onto that recorder. A live Pass with no callback SHALL be an empty run after its barriers and SHALL remain in that order. A DCE’d Pass SHALL NOT run even if it has a callback. Callbacks SHALL be `void(IFrameGraphRecorder&)` and SHALL receive the same recorder `execute` was given. `execute` SHALL NOT compile, SHALL NOT allocate, SHALL NOT call `planBarriers()`, SHALL NOT map to `VkImageLayout`, SHALL NOT call `vkCmdPipelineBarrier`, SHALL NOT begin, end, or submit a command buffer, and SHALL NOT run as a Job. A Vulkan recorder outside the graph headers MAY map those rows and MAY call `vkCmdPipelineBarrier`. At the start of a successful execute, execute SHALL snapshot live Pass order, the barrier list, and callbacks so a forbidden Setup mutation does not UAF; remaining Passes SHALL still record from that copy and execute SHALL still return success. Callbacks MUST NOT throw and MUST NOT reenter; `execute` SHALL NOT catch. `execute` SHALL NOT throw for graph-state failures. Tests of this capability SHALL still run without a Vulkan device.

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
- **AND** viewport present and forward mesh draws SHALL be unchanged

### Requirement: Vulkan recorder lives outside the graph headers
A Vulkan Frame graph recorder SHALL implement `IFrameGraphRecorder` in a compilation unit that is not `frame_graph.h` or `frame_graph.cpp`. That recorder MAY map Frame graph resource state onto `VkImageLayout` and MAY call `vkCmdPipelineBarrier`. Graph `execute` SHALL still not include `vulkan.h` and SHALL still not call `vkCmdPipelineBarrier`. Tests of this capability SHALL still pass a stand-in recorder and SHALL NOT create a Vulkan device.

#### Scenario: Graph tests stay Dummy
- **WHEN** `frame_graph_test` runs execute cases
- **THEN** it SHALL pass a Dummy recorder
- **AND** it SHALL NOT create a Vulkan device
- **AND** it SHALL NOT include `vulkan.h` from `frame_graph.h`

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

### Requirement: Dummy Deferred depth handshake has no DepthAttachment→Sampled barrier before Lighting
Tests of this capability SHALL compile, allocate, plan, and execute a Dummy graph that models editor Deferred: External color and depth (and a shadow Read on Lighting), a G-buffer Pass that Writes depth as DepthAttachment then Reads Sampled and does not declare color, a Lighting Pass that Reads depth as Sampled, Reads shadow as Sampled, and Writes color as ColorAttachment then Reads Sampled, and a Copy Sink that Reads color as Sampled. Live Pass order SHALL be G-buffer then Lighting then Copy. Dummy SHALL NOT log a barrier whose `before` is the Lighting Pass, whose `from` usage is DepthAttachment, and whose `to` usage is Sampled. Copy SHALL be a Sink. The G-buffer Pass SHALL NOT be a Sink. Tests SHALL NOT create a Vulkan device and SHALL NOT include `vulkan.h` from `frame_graph.h`.

#### Scenario: Deferred Dummy live order and depth handshake
- **WHEN** that Dummy graph executes
- **THEN** live Pass order SHALL be G-buffer then Lighting then Copy
- **AND** Copy SHALL be a Sink
- **AND** the G-buffer Pass SHALL NOT be a Sink
- **AND** no Dummy barrier whose `before` is the Lighting Pass SHALL have DepthAttachment as the `from` usage and Sampled as the `to` usage
- **AND** the test SHALL NOT create a Vulkan device

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
