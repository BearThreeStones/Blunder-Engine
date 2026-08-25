## MODIFIED Requirements

### Requirement: Hierarchy Panel row hit testing
A left pointer down on a visible Hierarchy Panel row, including the Hierarchy Line gutter and an empty expand-chevron slot, SHALL select that entity. A left pointer down on the expand chevron of a row that has children SHALL toggle expand/collapse. A right pointer down anywhere on a visible row (name, Hierarchy Line gutter, expand chevron, empty chevron slot) SHALL select that entity as a single selection and SHALL NOT toggle expand/collapse. The Hierarchy Line SHALL NOT be a separate control and SHALL NOT start a reparent drag.

#### Scenario: Click on gutter selects
- **WHEN** the user presses the left pointer on a row's Hierarchy Line gutter
- **THEN** that entity becomes the selection

#### Scenario: Click on empty chevron slot selects
- **WHEN** the user presses the left pointer on the empty expand-chevron slot of a leaf row
- **THEN** that entity becomes the selection

#### Scenario: Click on chevron toggles expand
- **WHEN** the user presses the left pointer on the expand chevron of a row that has children
- **THEN** that row toggles between expanded and collapsed

#### Scenario: Right-click on chevron does not toggle expand
- **WHEN** the user presses the right pointer on the expand chevron of a collapsed row that has children
- **THEN** that entity becomes the selection and the row does not expand from that press
