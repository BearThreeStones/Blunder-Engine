## ADDED Requirements

### Requirement: Light Add Remove and property Commands
Successfully adding Light, removing Light, or committing Inspector Light Component fields (type, color, intensity, enabled, contribution, range, cone, Area size, linking list) SHALL push Document History Commands targeted by EntityId. Undo of Add Light SHALL leave the entity with no Light Component.

#### Scenario: Undo Add Light
- **WHEN** the author adds Light from Add… and then undoes
- **THEN** that entity has no Light Component

#### Scenario: Undo Light type change
- **WHEN** the author changes a Light Component from Directional to Point and then undoes
- **THEN** the Light Component type is Directional again
