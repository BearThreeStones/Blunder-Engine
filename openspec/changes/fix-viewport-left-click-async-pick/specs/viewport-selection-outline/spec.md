## MODIFIED Requirements

### Requirement: Selection overlay updates after viewport pick

When viewport pick changes the primary selection, the outline and transform gizmo SHALL become visible without requiring camera movement and SHALL remain visible until selection changes or clears.

#### Scenario: Outline after first left-click with static camera

- **WHEN** the user left-clicks to select a mesh entity (sync-then-async path) and does not move the editor camera
- **THEN** the orange selection outline SHALL appear on the next viewport frame after click release without requiring a camera move or a second click

#### Scenario: Transform gizmo after first left-click with static camera

- **WHEN** the user left-clicks to select a mesh entity (sync-then-async path) and does not move the editor camera
- **THEN** the transform gizmo SHALL appear on the next viewport frame after click release without requiring a camera move or a second click

#### Scenario: Outline persists after piercing menu selection

- **WHEN** the user selects an entity from the piercing menu
- **THEN** the outline and transform gizmo SHALL appear and SHALL remain visible on subsequent static-camera frames without requiring another click or camera move
