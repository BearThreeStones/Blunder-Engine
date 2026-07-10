## ADDED Requirements

### Requirement: Piercing menu lists scene-root-child entities

When the user Ctrl+right-clicks inside the editor viewport, the engine SHALL gather all pickable entities at the click pixel (entity-ID peel, front-to-back), promote each hit to the scene-root child, deduplicate, and present a modal menu listing each promoted entity by display name.

#### Scenario: Multiple overlapping meshes in pick_test

- **WHEN** the user Ctrl+right-clicks a viewport pixel where `BoxFront`, `BoxMid`, and `BoxBack` overlap along the view ray
- **THEN** the piercing menu SHALL show three rows named `BoxFront`, `BoxMid`, and `BoxBack` (not glTF intermediate names such as `node`)

#### Scenario: Menu row selects promoted entity

- **WHEN** the user chooses a piercing menu row for entity `R`
- **THEN** `EditorSelectionSystem` primary selection SHALL be `R`
- **AND** Hierarchy SHALL highlight the row for `R`

#### Scenario: glTF nested mesh in menu

- **WHEN** Ctrl+right-click hits a glTF leaf `node_prim0` under scene root `BoxFront`
- **THEN** the menu row entity id and label SHALL refer to `BoxFront`

### Requirement: Piercing menu promotion matches viewport pick

Piercing menu entity ids and cached cycle lists SHALL use the same scene-root-child `promotePickEntity` walk-up as viewport left-click selection.

#### Scenario: Consistent promotion with left-click

- **WHEN** the user Ctrl+right-clicks and then left-clicks the same pixel without modifiers
- **THEN** click-cycle and menu selection SHALL reference the same promoted entity ids as sync-then-async left-click
