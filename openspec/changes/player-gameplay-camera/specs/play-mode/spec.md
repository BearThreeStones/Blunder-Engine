## MODIFIED Requirements

### Requirement: Play start camera gate

Play Mode start SHALL run camera preflight on the Play entry scene after the scene path is known and before Player spawn. Failure SHALL leave the editor in a non-Playing state for that attempt (no Player process).

#### Scenario: Preflight runs before spawn

- **WHEN** the user starts Play and the entry scene has no Camera
- **THEN** the engine SHALL log/report the camera gate error and SHALL NOT spawn Player

### Requirement: Play Pause does not unlock Editor Camera

While Play is paused, the Player view SHALL remain the scene Camera resolve path and SHALL NOT enable Editor Camera orbit or authorship input.

#### Scenario: Pause keeps authorship off

- **WHEN** Play is paused and the user attempts Editor Camera orbit in the Player window
- **THEN** the view SHALL NOT orbit via Editor Camera
