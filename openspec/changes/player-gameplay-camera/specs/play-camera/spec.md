## ADDED Requirements

### Requirement: Camera Component on scene entities

The engine SHALL support a native Camera Component on scene entities with vertical FOV (degrees), near clip, far clip, and an `isMain` flag. The component SHALL persist in scene JSON under a `"camera"` object and SHALL attach on scene load into `SceneInstance`.

#### Scenario: Serialize and deserialize Main Camera

- **WHEN** an entity definition has a camera with FOV, near, far, and `isMain: true`
- **THEN** round-trip through the scene serializer SHALL preserve those fields and `has_camera`

#### Scenario: Absent camera key

- **WHEN** an entity JSON object has no `"camera"` key
- **THEN** the loaded definition SHALL have `has_camera == false`

### Requirement: Resolve Play Camera

Given zero or more Camera Components in a scene, the engine SHALL resolve the Play view camera by preferring any camera with `isMain`, otherwise the first valid camera in iteration order. Empty input SHALL yield a not-ok result.

#### Scenario: Prefer Main

- **WHEN** two cameras exist and the second is Main
- **THEN** resolve SHALL select the second entity

#### Scenario: First when none Main

- **WHEN** two cameras exist and none is Main
- **THEN** resolve SHALL select the first entity

#### Scenario: Empty

- **WHEN** no cameras exist
- **THEN** resolve SHALL report not ok

### Requirement: Play camera preflight

Before spawning the Player process, Play start SHALL verify the Play entry scene contains at least one Camera Component. If none, Play SHALL abort without spawning Player and SHALL report that the entry scene has no Camera.

#### Scenario: Block Play without Camera

- **WHEN** the entry scene has no Camera Component
- **THEN** Play start SHALL fail and SHALL not spawn Player
