## ADDED Requirements

### Requirement: Placement Preview does not spawn
A visible Placement Preview SHALL NOT create a scene Entity and SHALL NOT push a Spawn Editor Command. Spawn remains sealed only on a successful Mesh Asset drop onto the viewport.

#### Scenario: Preview motion pushes no history
- **WHEN** the user drags a Mesh Asset over the viewport so Placement Preview follows the pointer and then cancels without dropping
- **THEN** Document History has no new Spawn Entity Command
