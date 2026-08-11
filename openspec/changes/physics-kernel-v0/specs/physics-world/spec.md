## ADDED Requirements

### Requirement: Physics World authority
The engine SHALL provide a **Physics World** that is the authoritative container for Kernel v0 body and collider state. The golden suite SHALL create, step, and assert against this World directly. Kernel v0 SHALL NOT require SceneInstance Entity TRS, ECS components, or Object nodes as the source of truth for body state.

#### Scenario: Headless world step
- **WHEN** a test constructs a Physics World, adds bodies, and calls step without an editor or scene
- **THEN** body poses update according to the simulation and can be read back from the World

### Requirement: Fixed Physics step
Physics World SHALL advance via an explicit fixed-timestep **step** API. The default step size SHALL be `dt = 1/60` second. Variable frame-rate accumulation SHALL NOT be required inside the World for v0.

#### Scenario: N fixed steps
- **WHEN** a World is stepped N times with `dt = 1/60`
- **THEN** exactly N integration/solve cycles run and golden assertions may key off N

### Requirement: SI units and default gravity
World quantities SHALL use SI conceptually (metres, seconds, kilograms) stored as Physics fixed scalars. Default gravity SHALL be `(0, 0, -9.81)` in engine Z-up space. A World SHALL allow overriding gravity.

#### Scenario: Free fall under default gravity
- **WHEN** a Dynamic body with no contacts is stepped under default gravity
- **THEN** its vertical motion follows gravity along −Z within golden tolerances in the fixed domain

### Requirement: RigidBody and Collider model
A **RigidBody** SHALL hold pose, velocity, mass properties, and a motion type, and MAY own zero or more **Colliders**. A **Collider** SHALL be a collision shape attached to a RigidBody with material parameters. Kernel v0 shapes SHALL include box, sphere, and capsule.

#### Scenario: One body one box collider
- **WHEN** a Dynamic RigidBody is created with a single box Collider and mass properties
- **THEN** the World simulates that body as a rigid body with that shape

### Requirement: Body motion types
Kernel v0 SHALL support **Dynamic**, **Static**, and **Kinematic** motion types. Static bodies SHALL be immovable (infinite mass). Dynamic bodies SHALL integrate forces/impulses. Kinematic bodies SHALL accept an external target pose; each step SHALL derive linear/angular velocity from the pose delta, use that velocity in contact so moving Kinematics can push Dynamics, and end the step at the target pose.

#### Scenario: Kinematic platform lifts Dynamic box
- **WHEN** a Kinematic body moves upward via successive target poses while contacting a Dynamic box
- **THEN** the Dynamic box is lifted without unbounded penetration relative to the golden tolerances

### Requirement: Physics material defaults
Each Collider SHALL carry friction and restitution parameters. Contact between two Colliders SHALL combine materials with a defined simple rule. Default restitution SHALL be `0` for stacking-friendly behavior.

#### Scenario: Resting contact with zero restitution
- **WHEN** a Dynamic box falls onto a Static box with default restitution 0
- **THEN** after sufficient steps the Dynamic body comes to rest (sleep or speed below threshold) without sustained bouncing

### Requirement: Per-body sleep
A Dynamic RigidBody SHALL enter sleep when linear and angular speed stay below thresholds for N consecutive steps, and SHALL wake on contact impulse or applied force. Static and Kinematic bodies SHALL NOT sleep. Island-wide sleep SHALL NOT be required for v0.

#### Scenario: Sleep then wake on hit
- **WHEN** a sleeping Dynamic body is struck by another Dynamic body
- **THEN** the sleeping body wakes and participates in subsequent steps
