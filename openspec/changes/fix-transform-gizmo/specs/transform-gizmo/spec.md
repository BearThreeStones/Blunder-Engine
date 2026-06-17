## ADDED Requirements

### Requirement: Gizmo mode persists across selection changes

The editor SHALL preserve the active transform gizmo mode (none, translate, rotate, or scale) when the user selects a different scene entity. Changing selection MUST NOT automatically switch the gizmo to translate.

#### Scenario: Rotate mode preserved when selecting another entity

- **WHEN** the user activates Rotate mode and then selects a different entity in the hierarchy or viewport
- **THEN** the gizmo mode remains Rotate
- **AND** rotation dials appear at the newly selected entity's pivot
- **AND** the transform toolbar continues to show Rotate as active

#### Scenario: Scale mode preserved when selecting another entity

- **WHEN** the user activates Scale mode and then selects a different entity
- **THEN** the gizmo mode remains Scale
- **AND** scale handles appear at the newly selected entity's pivot

#### Scenario: Selection change redraws gizmo at new pivot

- **WHEN** the user changes selection while any gizmo mode is active
- **THEN** the viewport redraws
- **AND** the gizmo is positioned using the new selection's world transform

### Requirement: Translate gizmo shows axis arrows and plane handles

In translate mode with a selected entity, the viewport SHALL render all three axis arrows (X, Y, Z) and three plane handles (XY, YZ, ZX) when not faded by view alignment, plus the center view-aligned handle.

#### Scenario: Axis arrows visible with plane handles

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** colored axis arrows extend from the gizmo pivot along each visible axis
- **AND** plane handles are visible at the axis corners

#### Scenario: Translate axis drag moves entity

- **WHEN** the user drags a translate axis arrow
- **THEN** the selected entity moves along that axis
- **AND** the inspector position fields update live

### Requirement: Rotate gizmo shows interactive dials

In rotate mode with a selected entity, the viewport SHALL render rotation dials for X, Y, and Z axes and allow drag rotation.

#### Scenario: Rotation dials visible

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three rotation dials are drawn at the entity pivot

#### Scenario: Rotation dial drag updates entity

- **WHEN** the user drags a rotation dial
- **THEN** the selected entity rotates around the corresponding axis
- **AND** the inspector rotation fields update live

### Requirement: Scale gizmo is visible and interactive

In scale mode with a selected entity, the viewport SHALL render distinct scale handles (per-axis boxes and uniform center handle) and support drag scaling.

#### Scenario: Scale handles visible

- **WHEN** scale mode is active and an entity is selected
- **THEN** per-axis scale handles and a uniform scale handle are drawn at the pivot

#### Scenario: Axis scale drag updates entity

- **WHEN** the user drags a per-axis scale handle
- **THEN** the selected entity's scale changes along that axis only
- **AND** the inspector scale fields update live

#### Scenario: Uniform scale drag updates entity

- **WHEN** the user drags the uniform scale center handle
- **THEN** the selected entity's scale changes uniformly on all axes
- **AND** the inspector scale fields update live

### Requirement: Transform toolbar and shortcuts reflect engine mode

The Slint transform toolbar and keyboard shortcuts (G, R, S) SHALL stay synchronized with `TransformGizmoController` mode without overwriting user mode on selection change.

#### Scenario: Toolbar shows active mode after selection change

- **WHEN** the user is in Scale mode and selects a different entity
- **THEN** the toolbar Scale button remains highlighted

#### Scenario: Hotkey sets mode

- **WHEN** the user presses R with a selected entity
- **THEN** the gizmo switches to rotate mode
- **AND** the toolbar shows Rotate as active
