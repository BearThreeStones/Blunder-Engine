## MODIFIED Requirements

### Requirement: Player authorship isolation

The Player host SHALL NOT accept Editor Camera interaction, viewport pick, Transform/Navigate gizmo input, or other authorship shortcuts. The Player SHALL accept Gameplay Input when focused and not paused, plus system window chrome (e.g. close). Editor Overlay draw/interaction remains disabled in Player (existing).

#### Scenario: No Editor Camera orbit in Player

- **WHEN** the user uses RMB/MMB/orbit shortcuts in the Player window
- **THEN** the Player view SHALL NOT orbit via Editor Camera

#### Scenario: Player view from scene Camera

- **WHEN** Play is running and the entry scene has a resolvable Camera Component
- **THEN** each Player frame SHALL use that resolved view/projection, not Editor Camera matrices
