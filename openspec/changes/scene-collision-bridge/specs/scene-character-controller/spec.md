# Spec Delta

## Purpose

Authors attach a Character Controller Unique so gameplay walks with capsule sweep and slide instead of teleporting Object.Position through the world.

## ADDED Requirements

### Requirement: Character Controller Unique
The engine SHALL support a Character Controller Unique on a scene entity (at most one per entity). It SHALL author a vertical capsule (axis entity local +Z) with radius and full height in metres, plus slope limit in degrees, step height in metres, floor snap length in metres, skin width in metres, and a collision mask. Adding Character Controller SHALL NOT create a bound Object. The Unique SHALL NOT be a C# Behaviour.

#### Scenario: Round-trip Character Controller
- **WHEN** an entity has a Character Controller Unique with radius, height, slope, step, snap, skin, and mask
- **THEN** scene serialize and load preserve those fields

### Requirement: MoveAndSlide is the walk API
While Play is running, Character Controller motion SHALL move by capsule sweep against the Physics World using the Unique’s mask, then slide along blocking hits, and MAY snap to a walkable floor. Walkable up is world +Z. Gravity SHALL be applied by script to velocity, not by a hidden CMC. Writing Object position SHALL teleport without sweep and SHALL NOT be the product walk path.

#### Scenario: Slide along a wall
- **WHEN** Play is running and MoveAndSlide is called with velocity into a Static wall the mask sees
- **THEN** the entity slides along that wall
- **AND** it does not pass through the wall

#### Scenario: Floor stick
- **WHEN** the controller is on a Static floor and MoveAndSlide is called with horizontal velocity
- **THEN** the controller remains on that floor within snap length
- **AND** IsOnFloor is true

#### Scenario: Position write does not sweep
- **WHEN** Object position is set to the far side of a Static wall
- **THEN** the entity is placed at that position without a sweep
- **AND** that write is not required for walk

### Requirement: Areas do not block the controller
Character Controller slides SHALL NOT be blocked by Area colliders. Areas remain queryable through the physics query API.

#### Scenario: Ice area does not stop walk
- **WHEN** the controller MoveAndSlides through an Area named in group `TerrainIce`
- **THEN** the capsule is not stopped by that Area

### Requirement: Add… Character Controller defaults
Adding Character Controller from Add… SHALL create a capsule Character Controller Unique with a usable default radius and height in metres and a mask of all bits. Shape SHALL NOT appear as a separate Unique kind.

#### Scenario: Add Character Controller
- **WHEN** the author adds Character Controller to an entity that has none
- **THEN** that entity has a Character Controller Unique
- **AND** Add… Character Controller is disabled while it remains
