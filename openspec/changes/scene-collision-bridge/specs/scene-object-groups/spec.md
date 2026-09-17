# Spec Delta

## Purpose

Entities carry string gameplay groups so hits and find-by-name can identify TerrainIce, TerrainSnow, and LeashPivots without using those names as physics layers.

## ADDED Requirements

### Requirement: String groups on the entity
A scene entity SHALL store zero or more group name strings. Groups SHALL round-trip on the scene document. Groups SHALL NOT be a physics query mask. Duplicate names on one entity SHALL collapse to one membership.

#### Scenario: Groups round-trip
- **WHEN** an entity is saved with groups `TerrainIce` and `LeashPivots`
- **THEN** reload restores both names

#### Scenario: Groups are not a ray mask
- **WHEN** a ray mask excludes an entity’s collision layer
- **THEN** that entity is not hit even if it is in group `TerrainIce`

### Requirement: Hit exposes groups
A successful physics query hit SHALL include the hit entity’s group names.

#### Scenario: Ice area hit lists TerrainIce
- **WHEN** a ray with collide-with-areas true hits an Area whose entity groups include `TerrainIce`
- **THEN** the hit’s group list contains `TerrainIce`

### Requirement: Find Objects in a group
C# SHALL be able to list bound Objects whose entities are in a named group. Entities in that group with no bound Object SHALL be omitted from that Object list.

#### Scenario: Bound Object is found
- **WHEN** an Object’s entity is in group `LeashPivots`
- **THEN** find-by-group `LeashPivots` includes that Object

#### Scenario: Entity-only group is omitted from Object find
- **WHEN** an entity is in group `TerrainIce` and has no bound Object
- **THEN** find-by-group `TerrainIce` does not invent an Object for that entity
