# play-camera Specification

## Purpose
Scene Camera Component for Play view: persist/load on entities, resolve Main/first camera into view/projection, and fail Play preflight when the entry scene has no Camera.

## Requirements

### Requirement: Camera Component on scene entities

The engine SHALL support a native Camera Component on scene entities with vertical FOV (degrees), near clip, far clip, and an `isMain` flag. The component SHALL persist in scene JSON under a `"camera"` object and SHALL attach on scene load into `SceneInstance`.

#### Scenario: Serialize and deserialize Main Camera

- **WHEN** an entity definition has a camera with FOV, near, far, and `isMain: true`
- **THEN** round-trip through the scene serializer SHALL preserve those fields and `has_camera`

#### Scenario: Absent camera key

- **WHEN** an entity JSON object has no `"camera"` key
- **THEN** the loaded definition SHALL have `has_camera == false`

### Requirement: Resolve Play Camera

Given zero or more Camera Components in a scene, the engine SHALL resolve the Play view camera by preferring any camera with `isMain` whose Object is Active in Hierarchy, otherwise the first valid camera that is Active in Hierarchy in stable iteration order (ascending EntityId). Cameras that are not Active in Hierarchy SHALL NOT be selected. Empty input, or only inactive cameras, SHALL yield a not-ok result. The editor SHALL NOT fall back to the Editor Camera.

#### Scenario: Prefer Main

- **WHEN** two cameras exist and the second is Main
- **AND** both are Active in Hierarchy
- **THEN** resolve SHALL select the second entity

#### Scenario: First when none Main

- **WHEN** two cameras exist and none is Main
- **AND** both are Active in Hierarchy
- **THEN** resolve SHALL select the first entity in stable iteration order

#### Scenario: Empty

- **WHEN** no cameras exist
- **THEN** resolve SHALL report not ok

#### Scenario: Inactive Main is skipped

- **WHEN** the Main Camera is not Active in Hierarchy
- **AND** another Camera is Active in Hierarchy
- **THEN** resolve SHALL select that other Camera

#### Scenario: All cameras inactive

- **WHEN** every Camera in the scene is not Active in Hierarchy
- **THEN** resolve SHALL report not ok

### Requirement: Play camera preflight

Before spawning the Player process, Play start SHALL verify the Play entry scene contains at least one Camera Component whose Object is Active in Hierarchy. If none, Play SHALL abort without spawning Player and SHALL report that the entry scene has no Camera. The editor SHALL NOT fall back to the Editor Camera.

#### Scenario: Block Play without Camera

- **WHEN** the entry scene has no Camera Component
- **THEN** Play start SHALL fail and SHALL not spawn Player

#### Scenario: Block Play when every Camera is inactive

- **WHEN** the entry scene has Camera Components and none is Active in Hierarchy
- **THEN** Play start SHALL fail and SHALL not spawn Player
