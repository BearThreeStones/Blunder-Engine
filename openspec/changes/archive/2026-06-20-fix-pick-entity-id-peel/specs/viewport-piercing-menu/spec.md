## ADDED Requirements

### Requirement: Piercing menu modal mouse capture

The piercing menu SHALL behave as a modal overlay for viewport mouse input while visible.

#### Scenario: Viewport pick deferred while menu open

- **WHEN** the piercing menu is visible
- **THEN** left-click viewport mesh pick and same-pixel cycle gestures SHALL NOT run until the menu is dismissed or an item is chosen

#### Scenario: Menu selection stable in hierarchy

- **WHEN** the user chooses a row in the piercing menu
- **THEN** the hierarchy primary selection SHALL match the chosen entity and SHALL NOT change on the same pointer gesture due to viewport pick cycling
