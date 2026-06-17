## MODIFIED Requirements

### Requirement: Translate gizmo shows axis arrows and plane handles

In translate mode with a selected entity, the viewport SHALL render all three axis arrows (X, Y, Z) and three plane handles (XY, YZ, ZX) when not faded by view alignment, plus the center view-aligned handle. Axis arrows MUST be clearly visible (not sub-pixel or fully occluded) under normal editor camera distances.

#### Scenario: Axis arrows visible with plane handles

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** colored axis arrows extend from the gizmo pivot along each visible axis
- **AND** plane handles are visible at the axis corners
- **AND** at least one axis arrow is visible from a default perspective camera view

#### Scenario: Translate axis drag moves entity

- **WHEN** the user drags a translate axis arrow
- **THEN** the selected entity moves along that axis
- **AND** the inspector position fields update live

### Requirement: Rotate gizmo shows interactive dials

In rotate mode with a selected entity, the viewport SHALL render rotation dials for X, Y, and Z axes and allow drag rotation. Dials MUST be visible (not empty) when the transform toolbar shows Rotate as active and an entity is selected.

#### Scenario: Rotation dials visible

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three rotation dials are drawn at the entity pivot
- **AND** at least one dial is clearly visible from a default perspective camera view

#### Scenario: Rotation dial drag updates entity

- **WHEN** the user drags a rotation dial
- **THEN** the selected entity rotates around the corresponding axis
- **AND** the inspector rotation fields update live

### Requirement: Scale gizmo is visible and interactive

In scale mode with a selected entity, the viewport SHALL render distinct scale handles (per-axis boxes and uniform center handle) and support drag scaling. Handles MUST be visible when the transform toolbar shows Scale as active and an entity is selected.

#### Scenario: Scale handles visible

- **WHEN** scale mode is active and an entity is selected
- **THEN** per-axis scale handles and a uniform scale handle are drawn at the pivot
- **AND** at least one scale handle is clearly visible from a default perspective camera view

#### Scenario: Axis scale drag updates entity

- **WHEN** the user drags a per-axis scale handle
- **THEN** the selected entity's scale changes along that axis only
- **AND** the inspector scale fields update live

#### Scenario: Uniform scale drag updates entity

- **WHEN** the user drags the uniform scale center handle
- **THEN** the selected entity's scale changes uniformly on all axes
- **AND** the inspector scale fields update live

## ADDED Requirements

### Requirement: Non-plane gizmo styles render in screen overlay pass

The transform gizmo overlay SHALL render all `GizmoDrawStyle` variants used in active modes (arrow, plane, center, dial, dial_ghost, scale_box) when their mode is active and an entity is selected. A style MUST NOT silently produce zero visible pixels due to pipeline depth rejection or degenerate geometry while other styles at the same pivot render correctly.

#### Scenario: Rotate mode issues dial draw calls

- **WHEN** rotate mode is active with a valid selection
- **THEN** the overlay records three dial draw submissions (one per axis)
- **AND** the resulting viewport image shows visible dial geometry at the pivot

#### Scenario: Gizmo mode change refreshes viewport

- **WHEN** the user switches transform mode via toolbar or G/R/S hotkey
- **THEN** the next presented viewport frame reflects the new gizmo geometry
