## ADDED Requirements

### Requirement: Behaviour-bearing scene entities bind an Object
An authored scene entity that declares one or more Behaviours SHALL materialize or reuse a bound Object for that entity’s EntityId so Behaviours have a ClassDB/Object serialization entry. Entities with an empty Behaviour list NEED NOT create an Object solely for scripting.

#### Scenario: Object bound when behaviours present
- **WHEN** a scene entity with a non-empty Behaviour list is instantiated
- **THEN** an Object exists with `getEntityId()` equal to that entity’s EntityId and hosts the declared Behaviour slots
