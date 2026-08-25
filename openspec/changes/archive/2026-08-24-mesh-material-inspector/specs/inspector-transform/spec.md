## MODIFIED Requirements

### Requirement: Local Transform Inspector UI
The Inspector SHALL present a collapsible Local Transform section with horizontal axis number fields for Position, Rotation, and Scale. Each field SHALL show an axis-colored label and a unit suffix where applicable (`m` for Position, `°` for Euler Rotation). The section SHALL use Blunder Inspector styling (not a Godot theme skin). Other Inspector sections SHALL remain unchanged by this Transform capability. Entity Inspector SHALL NOT host Editor shading overrides; that surface is retired.

#### Scenario: Transform section visible for a selection
- **WHEN** one or more entities are selected
- **THEN** the Inspector SHALL show the Local Transform section with Position, Rotation, and Scale controls

#### Scenario: No selection
- **WHEN** nothing is selected
- **THEN** the Inspector SHALL indicate no selection and SHALL NOT apply Transform edits
