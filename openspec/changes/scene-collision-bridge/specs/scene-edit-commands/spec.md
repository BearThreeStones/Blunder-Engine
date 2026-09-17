# Spec Delta

## ADDED Requirements

### Requirement: Collider and Character Controller Commands
Successfully adding Collider, removing Collider, adding Character Controller, removing Character Controller, or committing Inspector fields on those Uniques (shape, body kind, sizes, layer, mask, Character Controller capsule and slide fields, entity groups edited from those sections) SHALL push Document History Commands targeted by EntityId. Undo of Add Collider SHALL leave the entity with no Collider Unique. Undo of Add Character Controller SHALL leave the entity with no Character Controller Unique.

#### Scenario: Undo Add Collider
- **WHEN** the author adds Collider from Add… and then undoes
- **THEN** that entity has no Collider Unique

#### Scenario: Undo Add Character Controller
- **WHEN** the author adds Character Controller from Add… and then undoes
- **THEN** that entity has no Character Controller Unique

#### Scenario: Undo Collider shape change
- **WHEN** the author changes a Collider Unique from Box to Sphere and then undoes
- **THEN** the Collider Unique shape is Box again
