# Spec Delta

## Purpose

Blunder.Api exposes physics queries, entity groups, and Character Controller MoveAndSlide through the registered NativeAbi so Play scripts do not teleport through the world.

## ADDED Requirements

### Requirement: Physics query façade
`Blunder.Api` SHALL expose raycast, box shapecast, sphere shapecast, and capsule shapecast taking origin/direction or poses, a max distance in metres, a layer mask, and collide-with-areas. Calls SHALL go through the registered NativeAbi table. Results SHALL be in metres.

#### Scenario: Managed ray uses registered pointers
- **WHEN** ScriptHost has registered NativeAbi and a Behaviour raycasts
- **THEN** the call goes through the registered physics query pointer
- **AND** it MUST NOT silently DllImport a second ObjectDB image

### Requirement: Character Controller façade
`Blunder.Api` SHALL expose a Character Controller type obtained from the host Object that has a Character Controller Unique. It SHALL provide Velocity, MoveAndSlide, and floor/wall/ceiling flags matching the native Unique.

#### Scenario: MoveAndSlide from Tick
- **WHEN** a mounted Behaviour sets Velocity and calls MoveAndSlide during Play Tick
- **THEN** the entity’s pose updates by capsule sweep and slide
- **AND** Object.Position is not the required walk call

### Requirement: Group façade on Object
When an Object is bound to an entity, `Blunder.Api` SHALL expose add/remove/query of that entity’s groups and find-Objects-in-group. Find SHALL return only bound Objects.

#### Scenario: IsInGroup after add
- **WHEN** a Behaviour adds group `LeashPivots` on its Object
- **THEN** IsInGroup `LeashPivots` is true on that Object
