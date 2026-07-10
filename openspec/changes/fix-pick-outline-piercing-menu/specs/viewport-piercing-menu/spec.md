## ADDED Requirements

### Requirement: Compact piercing menu panel

The piercing menu visible panel SHALL size to its row content and SHALL NOT expand to fill the editor window. A separate full-window transparent dismiss layer MAY capture clicks outside the panel.

#### Scenario: Panel fits row count

- **WHEN** the piercing menu opens with N rows (N ≥ 1) at a viewport pixel
- **THEN** the visible menu panel height SHALL equal the stacked row layout (padding + N × row height) and SHALL NOT cover unrelated dock panels such as the Inspector

#### Scenario: Dismiss outside panel

- **WHEN** the piercing menu is visible and the user clicks outside the compact panel
- **THEN** the menu SHALL dismiss without changing selection

#### Scenario: Overlapping meshes still listed

- **WHEN** Ctrl+right-click hits three promoted entities at one pixel on `pick_test.scene.asset`
- **THEN** the menu SHALL show three rows with correct display names in a compact panel at the cursor

## MODIFIED Requirements

### Requirement: Piercing menu modal mouse capture

The piercing menu SHALL behave as a modal overlay for viewport mouse input while visible. The modal dismiss layer SHALL be visually transparent; only the compact menu panel SHALL have an opaque background.

#### Scenario: Viewport pick deferred while menu open

- **WHEN** the piercing menu is visible
- **THEN** left-click viewport mesh pick and same-pixel cycle gestures SHALL NOT run until the menu is dismissed or an item is chosen

#### Scenario: Menu selection stable in hierarchy

- **WHEN** the user chooses a row in the piercing menu
- **THEN** the hierarchy primary selection SHALL match the chosen entity and SHALL NOT change on the same pointer gesture due to viewport pick cycling
