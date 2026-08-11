## ADDED Requirements

### Requirement: Eight required Kernel v0 scenarios
The Physics golden suite SHALL include at least these required scenarios, each with numeric assertions in the fixed domain (or values derived without nondeterministic float): (1) free fall under gravity; (2) Dynamic resting on Static; (3) stack of ≥3 boxes; (4) sphere–box and capsule–box resting contacts; (5) frictional incline; (6) Kinematic platform lifts/pushes a Dynamic box; (7) impact against Static with bounded kinetic energy at restitution 0; (8) sleeping Dynamic wakes on hit.

#### Scenario: Suite enumerates all eight
- **WHEN** the golden suite test target runs
- **THEN** all eight required scenarios execute and must pass for v0 Done

### Requirement: Module unit tests
Isolated Fixed math and collision algorithms (e.g. GJK, a single contact solve step) SHALL have module unit tests in addition to whole-World goldens. Editor visual inspection SHALL NOT be the sole correctness gate.

#### Scenario: Fixed mul/div unit test
- **WHEN** Fixed math unit tests run
- **THEN** widening multiply/divide and basic ops assert exact expected Q32.32 bit patterns

### Requirement: Cross-platform bit-identical gate
For each required golden, running the same scenario on **Windows MSVC x64** and **Linux Clang or GCC x64** SHALL produce bit-identical asserted World state (poses, velocities, sleep flags as covered by the golden). Same-machine-only pass SHALL NOT satisfy this requirement.

#### Scenario: Dual-platform free fall
- **WHEN** scenario (1) is recorded on Win MSVC x64 and on Linux Clang/GCC x64
- **THEN** the compared fixed-state fields are bit-identical

### Requirement: Headless executable
The golden suite SHALL run as a headless automated test (or dedicated executable) without requiring the editor UI or Play Mode session.

#### Scenario: CI runs goldens
- **WHEN** CI invokes the physics golden test target
- **THEN** scenarios run to completion with pass/fail exit status and no GUI dependency

### Requirement: Optional calibration is non-authoritative
Early calibration dumps against a third-party engine MAY exist during development. Such dumps SHALL NOT be the lasting source of truth for Blunder physics correctness once goldens exist.

#### Scenario: Golden fails without third-party
- **WHEN** only Blunder goldens and module tests are present
- **THEN** v0 Done can be evaluated without a Jolt/Bullet/PhysX reference binary
