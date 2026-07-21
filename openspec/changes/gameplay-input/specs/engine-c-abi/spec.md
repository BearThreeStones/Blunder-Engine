## ADDED Requirements

### Requirement: Gameplay Input C-ABI entry points
The C-ABI SHALL expose entry points to read the current Gameplay Input Action snapshot: Move `(x, y)` floats and Jump pressed as a boolean integer, without requiring callers to include C++ class definitions.

#### Scenario: Read Move via C-ABI
- **WHEN** a caller invokes the Move getter with non-null out pointers
- **THEN** the call succeeds and writes the current Move components (idle zeros when Actions are inactive)

#### Scenario: Read Jump via C-ABI
- **WHEN** a caller invokes the Jump pressed getter with a non-null out pointer
- **THEN** the call succeeds and writes 0 or 1 for the current frame edge

### Requirement: ABI version for Gameplay Input table entries
When Gameplay Input NativeAbi fields are present, the C-ABI version constant SHALL be greater than or equal to 3.

#### Scenario: Version reports v3+
- **WHEN** a caller queries `blunder_engine_abi_version` after this change ships
- **THEN** the returned value is >= 3

## MODIFIED Requirements

### Requirement: ABI version for script host
The C-ABI version constant SHALL be greater than or equal to 3 when Behaviour, Vec3, and Gameplay Input entry points are present on the NativeAbi table.

#### Scenario: Version reports v3+
- **WHEN** a caller queries `blunder_engine_abi_version`
- **THEN** the returned value is >= 3
