## ADDED Requirements

### Requirement: Scene entities declare ordered Behaviours
A scene entity definition SHALL optionally include an ordered list of Behaviour declarations. Each declaration SHALL include a CLR type name string and a BehaviourId. An optional properties object of simple JSON values (bool, number, string) MAY be present.

#### Scenario: Round-trip type list
- **WHEN** a scene entity is saved with two Behaviours of types A and B with distinct BehaviourIds
- **THEN** deserializing the scene restores the same order, type names, and BehaviourIds

#### Scenario: Missing behaviours key
- **WHEN** a legacy scene entity has no `behaviours` field
- **THEN** deserialize succeeds with an empty Behaviour list

### Requirement: Load creates Object slots and mounts peers when host running
On scene instantiate, each entity with a non-empty Behaviour list SHALL have a bound Object whose Behaviour slots match the declared list. WHEN DotNetHost is running and the Project game assembly is loaded, the engine SHALL attach managed peers for each slot in list order.

#### Scenario: Mount with host running
- **WHEN** a scene with a Behaviour declaration is loaded and DotNetHost is running with the declaring type available
- **THEN** the Object’s Behaviour has a non-null Script Peer and invokeTick reaches the managed Behaviour

#### Scenario: Load without host
- **WHEN** the same scene loads without DotNetHost running
- **THEN** Object Behaviour slots exist with correct type names and null peers, and scene load still succeeds

### Requirement: Save exports Behaviours from bound Objects
Scene export/save SHALL write Behaviour declarations from the Object bound to each exported entity. Soft-deleted entities SHALL remain omitted from the written scene (existing tombstone rule).

#### Scenario: Export after attach
- **WHEN** an entity’s Object has Behaviours attached at runtime and the user saves the scene
- **THEN** the written scene asset includes those Behaviours’ type names and BehaviourIds in list order
