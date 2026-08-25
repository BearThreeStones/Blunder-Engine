## ADDED Requirements

### Requirement: Project Manager uses Editor Theme without Hub layout
Project Manager visual colors SHALL follow the Editor Theme. Create, Import, Open, and Remove SHALL use Editor controls; Open and New Project SHALL be accent primaries. Create/Import dialogs SHALL use Editor modal chrome. List rows SHALL read as hairline cards whose selected state uses the Editor accent. Spatial rhythm SHALL remain the Godot-shaped layout specified by Godot-shaped Project Manager chrome. The window SHALL NOT become a Unity Hub layout.

#### Scenario: Selected project uses accent
- **WHEN** a listed Project is selected
- **THEN** its row uses an accent-derived selection treatment

#### Scenario: Open is the accent primary
- **WHEN** a Project is selected
- **THEN** Open is an accent primary Editor controls Button
- **AND** Remove is a non-primary Editor controls Button

#### Scenario: Layout is still Godot-shaped
- **WHEN** the user views the Project Manager
- **THEN** Projects header, Create/Import strip, list, Open/Remove side column, and status footer remain in that arrangement
