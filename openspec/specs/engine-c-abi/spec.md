# engine-c-abi Specification

## Purpose
Versioned C-ABI surface for ClassDB/Object/PtrCall operations, shaped for future script hosts without requiring a managed runtime to link or start the engine.

## Requirements

### Requirement: Stable C-ABI for ClassDB and Objects
The engine SHALL expose a versioned C-ABI header that operates on ObjectId and opaque method-bind handles without requiring callers to include C++ class definitions or use C++ member pointers.

#### Scenario: Resolve Object by id
- **WHEN** a C caller passes a valid ObjectId to the C-ABI resolve/lookup entry point
- **THEN** the call succeeds and subsequent C-ABI operations can target that Object

#### Scenario: Invalid ObjectId
- **WHEN** a C caller passes an invalid or destroyed ObjectId
- **THEN** the C-ABI reports failure without crashing

### Requirement: PtrCall available through C-ABI
The C-ABI SHALL provide a PtrCall entry that accepts a method bind, instance identity, argument slot array, and optional return slot, matching the ClassDB PtrCall convention.

#### Scenario: Cross-language shaped call
- **WHEN** a C caller invokes PtrCall for a registered method with correctly typed slots
- **THEN** the native method executes as if invoked through ClassDB PtrCall

### Requirement: Behaviour operations on C-ABI
The C-ABI SHALL expose entry points to add/remove Behaviours on an Object by ObjectId, query Behaviour count and ids, and set/get a Behaviour Script Peer pointer, without exposing C++ Object layout.

#### Scenario: Add Behaviour via C-ABI
- **WHEN** a caller adds a Behaviour type name on a valid ObjectId through the C-ABI
- **THEN** a non-zero BehaviourId is returned and Behaviour count increases by one

#### Scenario: Set peer via C-ABI
- **WHEN** a caller sets a Script Peer pointer for a valid BehaviourId
- **THEN** get-peer returns that pointer until cleared or the Behaviour is removed

### Requirement: Vec3 properties through C-ABI
The C-ABI SHALL support get/set of Vec3 ClassDB properties (at least Object position) by ObjectId for managed bindings.

#### Scenario: Position round-trip via C-ABI
- **WHEN** a caller sets Object position to (1,2,3) through the C-ABI Vec3 setter and then gets it
- **THEN** the returned components match the written values

### Requirement: ABI version for script host
The C-ABI version constant SHALL be greater than or equal to 2 when Behaviour and Vec3 entry points are present.

#### Scenario: Version reports v2+
- **WHEN** a caller queries `blunder_engine_abi_version`
- **THEN** the returned value is >= 2

### Requirement: Shared library export for DllImport
The build SHALL provide a SHARED library target (`blunder_engine_c`) that exports the C-ABI entry points for consumers that intentionally use that image as their sole ObjectDB owner (for example Approach A `dotnet_host_test`). In-process CoreCLR alongside a statically linked `engine_runtime` SHALL NOT rely on default `DllImport("blunder_engine_c")` for ObjectDB traffic.

#### Scenario: SHARED target builds
- **WHEN** the `blunder_engine_c` target is built
- **THEN** a shared library binary is produced that exports the versioned C-ABI symbols

#### Scenario: SHARED remains valid for single-image hosts
- **WHEN** a host process uses SHARED `blunder_engine_c` as its only ObjectDB image and registers that module’s exports into ScriptHost
- **THEN** managed Behaviour Tick works against Objects created through that image’s C-ABI

### Requirement: Lifecycle hooks registerable through C-ABI
The C-ABI SHALL expose register/clear entry points for type-level Ready and Tick hooks without requiring a managed host. When hooks are registered and an Object has Behaviour Script Peers, invokeReady/invokeTick SHALL call the hook once per non-null peer.

#### Scenario: Ready hook via C-ABI
- **WHEN** a native caller sets a Ready hook through the C-ABI and invokes Ready on an Object with a Behaviour Script Peer
- **THEN** the registered hook runs with that peer

#### Scenario: Multiple peers with Tick hook
- **WHEN** a Tick hook is registered and an Object has two Behaviour peers
- **THEN** invokeTick calls the hook twice, once per peer

### Requirement: No managed host required to link C-ABI
The Reflection kernel build SHALL compile and link the C-ABI without requiring a .NET runtime. Absence of a script host SHALL leave lifecycle hooks empty/default rather than failing engine startup.

#### Scenario: Engine starts without script host
- **WHEN** the engine starts with ClassDB and C-ABI registered but no script host loaded
- **THEN** startup succeeds and Objects can still be created and have properties set through native/C-ABI APIs

### Requirement: In-process CoreCLR must not dual-load ObjectDB
When a process already links `engine_runtime` (editor, runtime, or tests that own a process ObjectDB) and hosts CoreCLR, managed C-ABI traffic SHALL use the process-linked ObjectDB. Loading SHARED `blunder_engine_c` as an additional ObjectDB owner in that process for ScriptHost/Api calls is forbidden.

#### Scenario: No second ObjectDB for managed calls in editor
- **WHEN** the editor hosts ScriptHost and Blunder.Api performs Object/Behaviour C-ABI operations
- **THEN** those operations resolve through the process-linked C-ABI (registered pointers) and MUST NOT create Objects only visible inside a separately loaded SHARED `blunder_engine_c` image
