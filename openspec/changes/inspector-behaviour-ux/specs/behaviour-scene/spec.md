## ADDED Requirements

### Requirement: Object Behaviour slots carry property bags
Each Behaviour slot on an Object SHALL store the bool/number/string property bag for that declaration. Scene instantiate SHALL copy bags from scene declarations onto restored slots. Scene export/save SHALL write bags from Object slots.

#### Scenario: Export preserves Inspector bag
- **WHEN** an Object Behaviour slot has properties set in Edit Mode and the author saves the scene
- **THEN** the written `behaviours` entry includes those properties

#### Scenario: Reload restores bag to slots
- **WHEN** a scene with Behaviour properties is instantiated without DotNetHost
- **THEN** Object slots have null peers and property bags matching the scene declarations

### Requirement: First Behaviour ensures bound Object
Adding the first Behaviour to an entity that has no bound Object SHALL create and bind an Object for that EntityId before inserting the Behaviour slot.

#### Scenario: Add on bare entity
- **WHEN** a selected entity has no bound Object and the author Adds a Behaviour
- **THEN** a bound Object exists for that EntityId with one Behaviour slot
