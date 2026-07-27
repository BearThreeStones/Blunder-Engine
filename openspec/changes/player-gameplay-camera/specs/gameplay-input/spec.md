## MODIFIED Requirements

### Requirement: Player input surface

In `EngineHostMode::Player`, gameplay-facing input sampling SHALL remain Gameplay Input (focused, not paused). Authorship input paths (Editor Camera, pick, gizmos) SHALL be excluded from the Player host regardless of focus.

#### Scenario: Focused Player gets Gameplay Input only

- **WHEN** the Player window is focused and Play is not paused
- **THEN** Gameplay Input MAY be sampled AND authorship input SHALL remain disabled
