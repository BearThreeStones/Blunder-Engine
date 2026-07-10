## ADDED Requirements

### Requirement: Editor multi-entity selection

The editor SHALL support selecting multiple scene entities simultaneously via Hierarchy interaction with keyboard modifiers.

#### Scenario: Replace selection on click

- **WHEN** the user clicks an entity row in the Hierarchy without Shift or Ctrl held
- **THEN** the editor SHALL replace the current selection with that entity as the sole selected entry and primary selection

#### Scenario: Add to selection with Shift

- **WHEN** the user Shift-clicks an entity row in the Hierarchy
- **THEN** the editor SHALL add that entity to the selection set without removing existing selected entities

#### Scenario: Toggle selection with Ctrl

- **WHEN** the user Ctrl-clicks an entity row that is already selected
- **THEN** the editor SHALL remove that entity from the selection set

#### Scenario: Primary selection for inspector

- **WHEN** multiple entities are selected
- **THEN** the most recently added or clicked entity SHALL be the primary selection used by the Inspector and transform gizmo pivot

### Requirement: Multi-select outline prepass

When multiple entities are selected, the outline prepass SHALL draw all mesh surfaces belonging to each selected entity's subtree into the object-ID buffer in a single prepass pass.

#### Scenario: Two mesh entities selected

- **WHEN** the user selects two separate mesh entities via Shift-click in the Hierarchy
- **THEN** the viewport SHALL display silhouette outlines around both meshes

#### Scenario: No seam between co-selected touching meshes

- **WHEN** two co-selected mesh surfaces share a boundary in screen space and belong to the same selection color group
- **THEN** the resolve pass SHALL NOT draw an outline edge along that internal boundary

### Requirement: Per-object ID encoding

The outline prepass SHALL write a packed 16-bit object identifier per pixel combining `color_id` (high bits) and `ob_id` (low bits). Each selected root entity SHALL receive a unique `ob_id` for the frame; all mesh draws in that entity's subtree SHALL share the same `ob_id`.

#### Scenario: Unique ID per selected root

- **WHEN** three root entities are selected
- **THEN** the prepass SHALL assign three distinct non-zero `ob_id` values (one per root) for their respective subtree mesh draws

#### Scenario: Background pixels

- **WHEN** no selected mesh covers a screen pixel
- **THEN** the object-ID buffer SHALL contain `0` at that pixel
