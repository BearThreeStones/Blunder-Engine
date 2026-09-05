# hierarchy-panel Specification

## Purpose
Gives authors a scannable Hierarchy Panel: left-aligned visible Scene Tree rows and Hierarchy Line guides that show parent/child among those rows.
## Requirements
### Requirement: Left-aligned Hierarchy Panel names
The Hierarchy Panel SHALL left-align each visible entity name. Names at the same depth SHALL share one left edge. A row with no children SHALL keep an empty expand-chevron slot so its name still shares that edge.

#### Scenario: Names are not centered
- **WHEN** the Hierarchy Panel lists one or more entities
- **THEN** each entity name starts immediately after its expand-chevron slot rather than in the horizontal center of the panel

#### Scenario: Leaf name aligns with sibling that has children
- **WHEN** a parent is expanded and one visible child has children and a sibling child has none
- **THEN** both child names share the same left edge

### Requirement: Hierarchy Line grammar
The Hierarchy Panel SHALL draw a Hierarchy Line in the gutter of visible entity rows. For each expanded parent, a vertical stem SHALL run under that parent's expand-chevron column through its visible children, and each of those children SHALL have a horizontal tick from that stem into its own expand-chevron column. Nested expanded rows SHALL draw their own stem. A parent's stem SHALL end at that parent's last visible child and SHALL NOT run through grandchildren. A stem of a non-last sibling SHALL continue through that sibling's nested visible rows until that last child. Root entity rows SHALL have no incoming Hierarchy Line.

#### Scenario: Expanded parent shows stem and ticks
- **WHEN** a non-root entity with at least two visible children is expanded
- **THEN** a vertical stem appears under the parent's expand-chevron column and each child has a horizontal tick into its expand-chevron column

#### Scenario: Last-child stem does not cross grandchildren
- **WHEN** a parent's last visible child is expanded and that last child has visible children
- **THEN** the parent's stem ends on the last-child row and does not continue through the grandchildren; the grandchildren use only the last child's own stem

#### Scenario: Non-last ancestor stem continues through nested rows
- **WHEN** a non-last child is expanded and shows nested visible rows, and a later sibling of that child is still visible
- **THEN** the ancestor stem continues through those nested rows until the ancestor's last visible child

#### Scenario: Root entities have no incoming line
- **WHEN** two or more root entities are listed
- **THEN** no Hierarchy Line connects the scene title to those roots, and no incoming stem appears to the left of a root row

### Requirement: Scene title is panel chrome
The scene display name in the Hierarchy Panel SHALL remain a label above the entity list. It SHALL NOT be a tree parent of root entities.

#### Scenario: Scene title is not an expand row
- **WHEN** the Hierarchy Panel shows a scene display name and at least one root entity
- **THEN** the scene display name is not an expand/collapse tree row and root entities are not drawn as its Hierarchy Line children

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

### Requirement: Hierarchy Line color
Hierarchy Line SHALL use a muted gray that is quieter than expand chevrons and row names. The line color SHALL NOT change on a selected row and SHALL NOT use the selection text color.

#### Scenario: Selected row keeps the same line color
- **WHEN** a child row is selected under an expanded parent
- **THEN** that row's Hierarchy Line uses the same muted gray as unselected sibling rows

### Requirement: Hierarchy Active checkbox
Each Hierarchy entity row SHALL show a checkbox for that Object's Object Active. The row name SHALL use muted (grey) text when the Object is not Active in Hierarchy. Left pointer on the checkbox SHALL NOT toggle expand/collapse and SHALL NOT start Inline Rename. When the row is in the selection, the checkbox SHALL use the same align rule as Hierarchy Active toggle. When the row is not in the selection, the checkbox SHALL select that entity (single) and toggle that Object's Object Active.

#### Scenario: Checkbox shows local flag while name is grey
- **WHEN** a parent is Object Active off and a child is Object Active on
- **THEN** the child's checkbox is on
- **AND** the child's name is grey

#### Scenario: Checkbox does not expand
- **WHEN** the author presses the left pointer on the Active checkbox of a collapsed parent row
- **THEN** Object Active changes
- **AND** the row does not expand from that press

### Requirement: Hierarchy Active toggle key
While the pointer is over the Hierarchy Panel (docked or floating), A is not used by Inline Rename, and a selection exists, A SHALL run Hierarchy Active toggle. A SHALL NOT toggle Object Active when the pointer is over the viewport, Inspector, or Content Browser.

#### Scenario: A over Hierarchy toggles
- **WHEN** the pointer is over the Hierarchy Panel
- **AND** one Object is selected
- **AND** Inline Rename is not active
- **AND** the author presses A
- **THEN** that Object's Object Active flips

#### Scenario: A over viewport does not toggle
- **WHEN** the pointer is over the editor viewport
- **AND** an Object is selected
- **AND** the author presses A without right or middle mouse held
- **THEN** Object Active is unchanged

