# object-identity Specification

## Purpose
Object / ObjectId identity, Scene Tree ownership, optional Entity binding with lazy materialization, projected property surface, and at-most-one Script Peer.

## Requirements

### Requirement: Object has a stable ObjectId
Every Object SHALL have an ObjectId that is the public identity for serialization intent, C-ABI, and Script Peer rebinding. ObjectId SHALL be distinct from ECS EntityId.

#### Scenario: Create Object
- **WHEN** the runtime creates an Object
- **THEN** it receives a valid ObjectId that can be used to resolve that Object until it is destroyed

#### Scenario: EntityId not required
- **WHEN** an Object exists with no bound Entity
- **THEN** it still has a valid ObjectId and participates in the Scene Tree

### Requirement: Scene Tree is Object-owned
Parent/child hierarchy for authored scene structure SHALL live on Objects. When an Object has a bound Entity, transform parenting SHALL be projected from the Scene Tree into the data layer — parents SHALL NOT be dual-written as competing sources of truth.

#### Scenario: Reparent Object
- **WHEN** an Object with a bound Entity is reparented under another Object in the Scene Tree
- **THEN** the Object parent link updates and the projected transform parent relationship matches the Scene Tree

### Requirement: Optional Entity binding and lazy materialization
An Object MAY bind at most one EntityId. Writing a spatial property on an Object with no Entity SHALL spawn an Entity with Transform (or equivalent) and bind it. Destroying an Object that has a bound Entity SHALL despawn that Entity.

#### Scenario: Lazy create on position write
- **WHEN** position is set on an Object that has no Entity
- **THEN** an Entity with Transform is created, bound to the Object, and stores the written position

#### Scenario: Destroy despawns
- **WHEN** an Object with a bound Entity is destroyed
- **THEN** the bound Entity is despawned and the ObjectId becomes invalid for lookup

### Requirement: Object property surface projects to components
ClassDB-advertised properties on Object (and subclasses) SHALL be the editor/script-facing API. Spatial fields such as position SHALL read and write the Transform component through projected accessors rather than exposing Component-as-Object.

#### Scenario: Position round-trip
- **WHEN** position is set then get on an Object with a bound Transform
- **THEN** the returned value matches the stored Transform translation

### Requirement: At most one Script Peer
An Object SHALL host zero or one Script Peer handle. Script Peers SHALL NOT be stored on ECS components.

#### Scenario: Bind peer
- **WHEN** a Script Peer handle is set on an Object that had none
- **THEN** GetCSharpHandle (or equivalent) returns that handle until cleared

#### Scenario: Clear peer
- **WHEN** the Script Peer handle on an Object is cleared
- **THEN** lifecycle script dispatch for that Object does not run until a new peer is bound
