## ADDED Requirements

### Requirement: Hierarchy row icon strip hit testing
A left pointer down on a Hierarchy row icon without Alt SHALL select that entity and SHALL NOT toggle expand/collapse. A left pointer down on a Hierarchy row icon with Alt SHALL select that entity and SHALL follow **Attachment property preview** (open, close, or raise the card). A right pointer down on a Hierarchy row icon SHALL select that entity as a single selection and SHALL NOT open a preview and SHALL NOT toggle expand/collapse. The icon strip SHALL NOT start a reparent drag.

#### Scenario: LMB icon selects
- **WHEN** the user presses the left pointer on a Hierarchy row icon without Alt
- **THEN** that entity becomes the selection
- **AND** the row does not toggle expand from that press

#### Scenario: Right-click icon selects
- **WHEN** the user presses the right pointer on a Hierarchy row icon
- **THEN** that entity becomes the selection
- **AND** no attachment property preview opens from that press

## MODIFIED Requirements

### Requirement: Hierarchy Panel row hit testing
A left pointer down on a visible Hierarchy Panel row, including the Hierarchy Line gutter and an empty expand-chevron slot, SHALL select that entity. A left pointer down on the expand chevron of a row that has children SHALL toggle expand/collapse. A right pointer down anywhere on a visible row (name, Hierarchy Line gutter, expand chevron, empty chevron slot, Hierarchy row icons) SHALL select that entity as a single selection and SHALL NOT toggle expand/collapse. The Hierarchy Line SHALL NOT be a separate control and SHALL NOT start a reparent drag. Left pointer down on Hierarchy row icons SHALL follow **Hierarchy row icon strip hit testing** instead of the expand-chevron rule.

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
