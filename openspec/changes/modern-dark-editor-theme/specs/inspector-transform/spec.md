## MODIFIED Requirements

### Requirement: Local Transform Inspector UI
The Inspector SHALL present a collapsible Local Transform section with horizontal axis number fields for Position, Rotation, and Scale. Each field SHALL show an axis-colored label and a unit suffix where applicable (`m` for Position, `°` for Euler Rotation). The section header MAY use an Editor controls Foldout. The vector cells SHALL remain compact AxisNumberField controls (Godot cell rhythm, not Editor controls Numeric Field) whose fill is the Editor Theme recessed field surface. Other Inspector sections SHALL remain unchanged by this capability except for Editor Theme Window fill / default text and Inspector control skin (Foldout / Add… / Remove) specified elsewhere.

#### Scenario: Transform section visible for a selection
- **WHEN** one or more entities are selected
- **THEN** the Inspector SHALL show the Local Transform section with Position, Rotation, and Scale controls

#### Scenario: No selection
- **WHEN** nothing is selected
- **THEN** the Inspector SHALL indicate no selection and SHALL NOT apply Transform edits

#### Scenario: Cells stay compact AxisNumberField
- **WHEN** a single entity is selected
- **THEN** Position X/Y/Z are compact axis-colored cells rather than Editor controls Numeric Fields
