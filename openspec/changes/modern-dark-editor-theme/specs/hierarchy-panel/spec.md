## ADDED Requirements

### Requirement: Hierarchy selection color follows Editor accent
The Hierarchy selected row SHALL use a fill and text color derived from the Editor accent token (soft fill over Window, accent-tinted text). It SHALL NOT use the authored `#3a3a4a` / `#bde7ff` pair. Row geometry (22px), gutter, chevrons, Hierarchy Line (`#737373`), and the square selection rectangle SHALL stay as authored. Hierarchy Line color SHALL still not change on the selected row (see Hierarchy Line color).

#### Scenario: Selected row uses accent, not the old pair
- **WHEN** a Hierarchy row is selected
- **THEN** its background is an accent-derived soft fill
- **AND** its name color is accent-tinted
- **AND** the selection rectangle is square
- **AND** the Hierarchy Line on that row stays the same muted gray as unselected siblings

#### Scenario: Row geometry is unchanged
- **WHEN** the Hierarchy lists an expanded parent and a selected child
- **THEN** row height, name left-alignment, and Hierarchy Line grammar match the existing Hierarchy Panel requirements
