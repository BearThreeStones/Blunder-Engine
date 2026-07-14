## ADDED Requirements

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

### Requirement: No managed host required to link C-ABI
The Reflection kernel build SHALL compile and link the C-ABI without requiring a .NET runtime. Absence of a script host SHALL leave lifecycle hooks empty/default rather than failing engine startup.

#### Scenario: Engine starts without script host
- **WHEN** the engine starts with ClassDB and C-ABI registered but no script host loaded
- **THEN** startup succeeds and Objects can still be created and have properties set through native/C-ABI APIs
