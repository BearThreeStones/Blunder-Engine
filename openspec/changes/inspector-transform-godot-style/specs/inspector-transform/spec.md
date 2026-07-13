## ADDED Requirements

### Requirement: Local Transform Inspector UI
The Inspector SHALL present a collapsible Local Transform section with horizontal axis number fields for Position, Rotation, and Scale. Each field SHALL show an axis-colored label and a unit suffix where applicable (`m` for Position, `°` for Euler Rotation). The section SHALL use Blunder Inspector styling (not a Godot theme skin). Other Inspector sections (for example shading) SHALL remain unchanged by this capability.

#### Scenario: Transform section visible for a selection
- **WHEN** one or more entities are selected
- **THEN** the Inspector SHALL show the Local Transform section with Position, Rotation, and Scale controls

#### Scenario: No selection
- **WHEN** nothing is selected
- **THEN** the Inspector SHALL indicate no selection and SHALL NOT apply Transform edits

### Requirement: Axis number field commit and scrub
Each axis number field SHALL accept keyboard entry that commits on Enter or focus loss, and SHALL support pointer scrubbing that applies live. Invalid keyboard text on commit SHALL restore the authoritative displayed value without writing a bad parse to the entity. While a field has an active keyboard draft, external Transform sync (including gizmo-driven sync) SHALL NOT overwrite that field’s draft text; Escape SHALL discard the draft and show the authoritative value again.

#### Scenario: Typed position commit
- **WHEN** one entity is selected and the user types a Position X value and presses Enter
- **THEN** that entity’s local position.x SHALL equal the parsed value
- **AND** the viewport SHALL update without requiring camera motion

#### Scenario: Live scrub
- **WHEN** the user pointer-scrubs a Position axis field
- **THEN** the corresponding local component SHALL update continuously during the scrub

#### Scenario: Focused field lock during gizmo edit
- **WHEN** a Transform field has keyboard focus with a draft
- **AND** a gizmo transform edit updates the selection
- **THEN** the focused field’s draft text SHALL remain unchanged until commit or Escape

### Requirement: Euler rotation under-sliders
In Euler Rotation Edit Mode, each rotation axis field SHALL include an under-slider that applies live to the entity’s authoritative Quaternion through the fixed engine Euler convention.

#### Scenario: Rotation slider live edit
- **WHEN** Euler mode is active and the user drags the Rotation X under-slider
- **THEN** the selected entity’s rotation SHALL update live via the fixed engine Euler compose path

### Requirement: Scale link
The Inspector SHALL provide a Scale link toggle (default on for the editor session). When Scale link is on and the user edits one scale axis to a new value V, each affected entity’s scale SHALL be updated with proportional scaling so that axis becomes V and the other axes keep their prior ratios to that axis. If the edited axis’s prior absolute value is near zero, only that axis SHALL change.

#### Scenario: Proportional scale link
- **WHEN** Scale link is on and an entity’s scale is (2, 1, 0.5)
- **AND** the user sets Scale X to 4
- **THEN** that entity’s scale SHALL become (4, 2, 1)

#### Scenario: Near-zero edited axis
- **WHEN** Scale link is on and Scale X is near zero
- **AND** the user sets Scale X to 2
- **THEN** only Scale X SHALL become 2

### Requirement: Rotation Edit Mode
The Inspector SHALL provide Rotation Edit Mode with Euler and Quaternion presentations. Changing mode SHALL NOT change how rotation is stored. Authoritative storage SHALL remain the entity Quaternion. Euler fields SHALL compose and decompose through the engine’s fixed SceneSerializer Euler convention. Quaternion component edits SHALL write a normalized Quaternion on commit. Rotation Edit Mode SHALL persist for the editor session across selection changes (default Euler).

#### Scenario: Switch to Quaternion and normalize
- **WHEN** a single entity is selected in Quaternion mode
- **AND** the user commits a non-unit Quaternion
- **THEN** the entity’s rotation SHALL be the normalized Quaternion
- **AND** the fields SHALL display the normalized components

#### Scenario: Euler edit writes Quaternion
- **WHEN** Euler mode is active and the user commits Euler angles
- **THEN** the entity’s stored rotation SHALL be the Quaternion composed from those angles with the fixed engine Euler convention

### Requirement: Multi-edit Absolute and Delta
When multiple entities are selected, the Inspector SHALL show a Multi-edit mode control choosing Absolute or Delta (default Absolute; session-persisted). Absolute mode SHALL write the committed component value to every selected entity. Delta mode SHALL treat the field baseline as 0 and add the committed or live delta to each selected entity’s current local component. When Absolute mode shows a component that differs across the selection, the field SHALL show a mixed placeholder until the user supplies a value. Multi-edit mode SHALL apply to Position and Scale. Rotation multi-edit SHALL use Delta only, applying the same Euler-angle delta through the fixed engine Euler convention onto each selected entity’s authoritative Quaternion. While multiple entities are selected, Quaternion component Absolute editing SHALL NOT be offered.

#### Scenario: Absolute multi-edit position
- **WHEN** multiple entities are selected with Multi-edit mode Absolute
- **AND** the user commits Position X = 5
- **THEN** every selected entity’s local position.x SHALL be 5

#### Scenario: Mixed Absolute placeholder
- **WHEN** multiple entities are selected with differing local Position X values and Multi-edit mode Absolute
- **THEN** the Position X field SHALL show a mixed placeholder rather than a single numeric value

#### Scenario: Delta multi-edit position
- **WHEN** multiple entities are selected with Multi-edit mode Delta
- **AND** the user commits Position X delta = 1
- **THEN** each selected entity’s local position.x SHALL increase by 1

#### Scenario: Delta multi-edit rotation
- **WHEN** multiple entities are selected
- **AND** the user applies a Rotation X delta of 10 degrees
- **THEN** each selected entity’s Quaternion SHALL be updated by that Euler X delta through the fixed engine Euler convention

### Requirement: Local space only
The Inspector Transform section SHALL edit local Position, Rotation, and Scale relative to each entity’s parent. This capability SHALL NOT provide a World/Global Transform editor.

#### Scenario: Edit is local
- **WHEN** the user changes Position in the Inspector
- **THEN** the written values SHALL be the entity’s local position components
