## MODIFIED Requirements

### Requirement: GraphBuilder records Setup
A caller SHALL record Frame graph Setup through GraphBuilder: create a Transient resource, import an External resource, add Passes that Read and Write Resources with a usage (Sampled, ColorAttachment, DepthAttachment, or Storage), and mark Sink Passes. Setup SHALL NOT parse JSON, SHALL NOT use a Blackboard, and SHALL NOT allocate GPU memory. `read`, `write`, or `markSink` on a Pass handle that was never added SHALL NOT throw; compile SHALL later return `InvalidPass`.

#### Scenario: Two-Pass Transient then Sink
- **WHEN** a test creates a Transient Texture, adds a Pass that Writes it, adds a Sink Pass that Reads it as Sampled, and compiles
- **THEN** compile SHALL succeed
- **AND** the live Pass order SHALL be the writer then the Sink
- **AND** that Transient’s Resource lifetime SHALL cover both Passes

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
- **WHEN** a test calls `read`, `write`, or `markSink` with a Pass handle that was never added
- **AND** the test compiles
- **THEN** compile SHALL return failure with reason `InvalidPass`
- **AND** compile SHALL NOT throw
- **AND** the live Pass order SHALL be empty

#### Scenario: Failure reasons follow priority
- **WHEN** a test’s Setup is invalid in more than one of: never-added Pass handle, dangling Resource, no Sink, live cycle
- **AND** the test compiles
- **THEN** compile SHALL return exactly one of those reasons, in this preference: `InvalidPass`, then `DanglingAccess`, then `NoSink`, then `Cycle`
- **AND** the live Pass order SHALL be empty
