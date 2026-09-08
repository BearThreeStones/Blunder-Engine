## Purpose

CPU Frame graph Setup and compile for one frame of GPU work: Passes, Transient and External Resources, dead-Pass elimination from Sinks, and Resource lifetimes — without allocating GPU memory or recording commands.

## ADDED Requirements

### Requirement: GraphBuilder records Setup
A caller SHALL record Frame graph Setup through GraphBuilder: create a Transient resource, import an External resource, add Passes that Read and Write Resources with a usage (Sampled, ColorAttachment, DepthAttachment, or Storage), and mark Sink Passes. Setup SHALL NOT parse JSON, SHALL NOT use a Blackboard, and SHALL NOT allocate GPU memory.

#### Scenario: Two-Pass Transient then Sink
- **WHEN** a test creates a Transient Texture, adds a Pass that Writes it, adds a Sink Pass that Reads it as Sampled, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the writer then the Sink
- **AND** that Transient’s Resource lifetime SHALL cover both Passes

### Requirement: Compile builds edges, order, DCE, and lifetimes
Frame graph compile SHALL derive producer/consumer edges from Resource access, produce a topological live Pass order, eliminate Passes that no Sink can reach, and record a Resource lifetime `[first access, last access]` on each live Resource. Compile SHALL NOT allocate GPU memory, alias heaps, insert barriers, or record commands. Compile SHALL NOT run as a Job.

#### Scenario: Unreachable post-process is dropped
- **WHEN** a test adds a Sink subgraph and a separate post-process Pass that Writes a Transient nobody Reads toward a Sink
- **AND** the test compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL omit that post-process Pass
- **AND** that Transient SHALL NOT be live

### Requirement: External resources are imported not created
An External resource SHALL be imported. Compile SHALL track Reads and Writes of it. The Frame graph SHALL NOT treat an External resource as a Transient it would allocate.

#### Scenario: Imported viewport-color stand-in
- **WHEN** a test imports an External Texture, adds a Sink Pass that Writes it as ColorAttachment, and compiles
- **THEN** compile SHALL succeed
- **AND** that Resource SHALL remain External
- **AND** compile SHALL NOT report it as a Transient the graph owns

### Requirement: Texture and Buffer share access rules
A Frame graph resource SHALL be a Texture or a Buffer. Create, import, Read, Write, compile edges, DCE, and Resource lifetime SHALL apply to both shapes. Usage SHALL NOT be a resource type. Attachment and Reference SHALL NOT be resource types.

#### Scenario: Buffer Transient write then read
- **WHEN** a test creates a Transient Buffer, adds a Pass that Writes it as Storage, adds a Sink Pass that Reads it as Storage, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the writer then the Sink
- **AND** that Buffer’s Resource lifetime SHALL cover both Passes

### Requirement: Compile returns failure without throwing
Compile SHALL NOT throw. A cycle among Passes that a Sink can reach, a Read or Write of a Resource that was never created or imported, or a graph with no Sink SHALL return failure and an empty live Pass order. Unreachable Passes SHALL NOT be a compile failure; DCE SHALL drop them, including unreachable Passes that cycle among themselves.

#### Scenario: Cycle fails compile
- **WHEN** a test builds Passes whose Resource accesses form a cycle that a Sink can reach
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

### Requirement: v1 does not execute the graph
v1 SHALL NOT resolve Frame graph handles to RHI objects, SHALL NOT replace ForwardRenderPath, and SHALL NOT record command buffers from the graph. GPU pick, Texture Loader copies, Mesh Preview, and Camera Preview SHALL NOT be Frame graph Passes in this slice. Bindless table entries SHALL NOT be Frame graph resources.

#### Scenario: Tests need no device
- **WHEN** `frame_graph_test` runs
- **THEN** it SHALL compile graphs without creating a Vulkan device
- **AND** viewport present and forward mesh draws SHALL be unchanged
