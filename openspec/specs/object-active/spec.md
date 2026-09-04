# object-active Specification

## Purpose

Persisted Object Active (Unity GameObject activeSelf) so authors can turn Objects off in the scene document, Play, and the editor viewport without deleting them or using Scene Visibility.

## Requirements

### Requirement: Object Active is local and persisted
Each scene Object SHALL have Object Active, default on. The field SHALL persist on the Scene Asset. Toggling a parent SHALL NOT rewrite descendants' Object Active. Object Active SHALL be distinct from Light enabled, SkeletonModifier enabled, and Delete tombstone. Absent JSON SHALL mean on.

#### Scenario: Default on
- **WHEN** a scene entity is created or loaded with no Object Active field
- **THEN** that Object is Object Active

#### Scenario: Round-trip inactive
- **WHEN** an Object is Object Active off and the scene is saved then loaded
- **THEN** that Object is still Object Active off
- **AND** its descendants' Object Active flags match the saved document

#### Scenario: Parent toggle keeps child flags
- **WHEN** a child is Object Active on and its parent is toggled off then on
- **THEN** the child remains Object Active on

### Requirement: Active in Hierarchy is derived
An Object SHALL be Active in Hierarchy when it is Object Active and every ancestor is Object Active and the Object is not tombstoned. Active in Hierarchy SHALL NOT be stored on the Scene Asset.

#### Scenario: Child under inactive parent
- **WHEN** a parent is Object Active off and a child is Object Active on
- **THEN** the child is not Active in Hierarchy
- **AND** the child's Object Active stays on

### Requirement: Inactive Objects stay in Hierarchy
An Object that is not Active in Hierarchy SHALL remain listed in the Hierarchy Panel and SHALL remain selectable there.

#### Scenario: Grey row still selectable
- **WHEN** an Object is not Active in Hierarchy
- **THEN** its Hierarchy row is still shown
- **AND** a left pointer down on that row selects it

### Requirement: Participation follows Active in Hierarchy
An Object that is not Active in Hierarchy SHALL NOT draw Mesh, SHALL NOT be viewport-pickable, SHALL NOT show selection outline or Transform gizmo, SHALL NOT contribute Light, SHALL NOT participate in Play Camera resolve, and SHALL NOT Tick Behaviours. Local Object Active on SHALL NOT override an inactive ancestor.

#### Scenario: Mesh leaves the viewport
- **WHEN** a MeshRenderer Object is Object Active off
- **THEN** that mesh is not drawn in the editor viewport or the Player

#### Scenario: Behaviours do not Tick
- **WHEN** an Object is not Active in Hierarchy
- **THEN** its Behaviours do not Tick

### Requirement: Object Active Command
Setting Object Active from Hierarchy Active toggle, Hierarchy Active checkbox, or Inspector Active checkbox SHALL seal one Document History Command for the whole selection. Undo SHALL restore each selected Object's prior Object Active. The Command SHALL be v1 Play-authorship-patchable.

#### Scenario: Undo restores flags
- **WHEN** the author turns two selected Objects off with A and then undoes
- **THEN** both Objects are Object Active on again

#### Scenario: No-op when nothing selected
- **WHEN** the pointer is over Hierarchy and A is pressed with an empty selection
- **THEN** Document History does not gain a Command

### Requirement: Multi-select aligns Object Active
When more than one Object is selected, Hierarchy Active toggle, Hierarchy Active checkbox (on a selected row), and Inspector Active checkbox SHALL set every selected Object's Object Active to one shared value: off if every selected Object is Object Active, otherwise on.

#### Scenario: Mixed selection turns on
- **WHEN** the selection contains one Object Active on Object and one Object Active off Object
- **AND** the author presses A with the pointer over Hierarchy
- **THEN** every selected Object is Object Active on

#### Scenario: All on turns off
- **WHEN** every selected Object is Object Active
- **AND** the author presses A with the pointer over Hierarchy
- **THEN** every selected Object is Object Active off

### Requirement: Inspector Active checkbox
The Inspector identity row SHALL show a checkbox for Object Active beside the Object name. It SHALL author the same field and Command as Hierarchy Active toggle. Mixed multi-select SHALL show an indeterminate checkbox.

#### Scenario: Inspector matches Hierarchy
- **WHEN** a single selected Object is Object Active off
- **THEN** the Inspector identity checkbox is off
- **AND** the Hierarchy row checkbox is off

### Requirement: Inline Rename does not toggle
While Hierarchy Inline Rename is active, A SHALL type the letter and SHALL NOT change Object Active.

#### Scenario: Rename types A
- **WHEN** Hierarchy Inline Rename is active and the author presses A
- **THEN** Object Active is unchanged
- **AND** the rename buffer receives A
