# object-identity Specification

## Purpose
Object / ObjectId identity, Scene Tree ownership, optional Entity binding with lazy materialization, projected property surface, and multi-Behaviour Script Peers (ADR 0011).
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

### Requirement: Object hosts Behaviours / Script Peers
An Object SHALL host zero or more Behaviours, each with at most one Script Peer handle. Script Peers SHALL NOT be stored on ECS components. BehaviourId SHALL identify a Behaviour instance for peer binding and SHALL be distinct from ObjectId.

#### Scenario: Bind peer on Behaviour
- **WHEN** a Script Peer handle is set on a Behaviour that had none
- **THEN** get-peer for that BehaviourId returns that handle until cleared or the Behaviour is removed

#### Scenario: Clear peer
- **WHEN** the Script Peer handle on a Behaviour is cleared
- **THEN** lifecycle script dispatch skips that Behaviour until a new peer is bound

#### Scenario: Multiple Behaviours allowed
- **WHEN** two Behaviours are added to the same Object
- **THEN** both remain addressable by distinct BehaviourIds concurrently

### Requirement: Behaviour-bearing scene entities bind an Object
An authored scene entity that declares one or more Behaviours SHALL materialize or reuse a bound Object for that entity’s EntityId so Behaviours have a ClassDB/Object serialization entry. Entities with an empty Behaviour list NEED NOT create an Object solely for scripting.

#### Scenario: Object bound when behaviours present
- **WHEN** a scene entity with a non-empty Behaviour list is instantiated
- **THEN** an Object exists with `getEntityId()` equal to that entity’s EntityId and hosts the declared Behaviour slots

