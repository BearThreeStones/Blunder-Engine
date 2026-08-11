## ADDED Requirements

### Requirement: Convex pair detection
The Physics Kernel SHALL detect collisions among v0 convex shapes (box, sphere, capsule) attached to bodies in the same World. Broadphase MAY be simple or hierarchical; pair processing order SHALL be deterministic for a given World state.

#### Scenario: Overlapping boxes generate contact
- **WHEN** two Dynamic boxes overlap after integration
- **THEN** the solver receives contact(s) for that pair in a stable order

### Requirement: Resting contact and stacking
Contact generation and resolution SHALL support resting contact on Static geometry and stable stacks of Dynamic convex bodies under default materials (restitution 0).

#### Scenario: Box stack remains upright
- **WHEN** three or more Dynamic boxes are stacked on a Static ground and stepped until sleep
- **THEN** the stack does not collapse and penetrations stay within golden bounds

### Requirement: Sphere and capsule resting contacts
The Kernel SHALL support resting contacts for sphere–box and capsule–box pairs at least as well as box–box for golden purposes.

#### Scenario: Sphere rests on box
- **WHEN** a Dynamic sphere is dropped onto a Static box
- **THEN** it comes to rest with bounded penetration

### Requirement: Frictional incline
Contact friction SHALL be sufficient for the incline golden: a Dynamic body on a Static incline SHALL NOT accelerate without bound; with adequate friction it SHALL stop or remain within a bounded speed for the authored angle.

#### Scenario: Friction holds on incline
- **WHEN** a Dynamic box rests on a Static incline at an angle where static friction should hold under the test material
- **THEN** after sufficient steps the box does not slide off indefinitely

### Requirement: Impact energy bound
For restitution 0 impacts against Static geometry, post-impact kinetic energy SHALL remain bounded per the golden (no explosive energy gain).

#### Scenario: Drop onto static floor
- **WHEN** a Dynamic body impacts a Static floor with restitution 0
- **THEN** measured kinetic energy after the impact sequence stays under the golden upper bound

### Requirement: Single-threaded deterministic solve
Kernel v0 collision detection and contact solve SHALL run single-threaded with a frozen, order-stable pipeline so identical World inputs yield bit-identical results across the P1 platform matrix.

#### Scenario: Same contacts same impulses
- **WHEN** the same World snapshot is stepped once on Win MSVC x64 and once on Linux Clang/GCC x64
- **THEN** resulting body pose/velocity fixed fields match bit-for-bit
