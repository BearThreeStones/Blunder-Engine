## ADDED Requirements

### Requirement: Transform gizmo matches Blender-style silhouettes

The transform gizmo overlay SHALL render Blender-like handle shapes in each active mode: translate uses cone-tipped axis arrows with small corner plane handles and a view-aligned center disc; rotate uses thin colored rings; scale uses axis-end cubes plus a uniform center cube. All styles MUST remain visible at default editor camera distances after styling changes.

#### Scenario: Move gizmo shows cone arrows and center disc

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** each visible axis shows a thin stem with a cone tip at the positive axis end
- **AND** plane handles appear as small squares at axis corners (not oversized blocks)
- **AND** the center handle is a white or neutral gray circular disc aligned to the view

#### Scenario: Rotate gizmo shows thin rings

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three colored rotation rings are drawn at the pivot
- **AND** ring tube thickness is visibly thinner than the prior visibility-fix thicken pass
- **AND** at least one ring is clearly visible from a default perspective camera view

#### Scenario: Scale gizmo shows axis-end cubes

- **WHEN** scale mode is active and an entity is selected
- **THEN** per-axis scale handles appear as small cubes at the positive end of each axis
- **AND** the uniform scale handle appears as a small cube at the pivot center
- **AND** scale handles are not rendered as flat plane quads

## MODIFIED Requirements

### Requirement: Translate gizmo shows axis arrows and plane handles

In translate mode with a selected entity, the viewport SHALL render all three axis arrows (X, Y, Z) and three plane handles (XY, YZ, ZX) when not faded by view alignment, plus the center view-aligned handle. Axis arrows MUST use cone-tipped arrow geometry (stem + tip), not thick cross-billboard blocks. Plane handles MUST use Blender-proportioned corner squares. The center handle MUST use circular disc geometry (`GizmoDrawStyle::center`).

#### Scenario: Axis arrows visible with plane handles

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** colored cone-tipped axis arrows extend from the gizmo pivot along each visible axis
- **AND** small plane handles are visible at the axis corners
- **AND** at least one axis arrow is visible from a default perspective camera view

#### Scenario: Translate axis drag moves entity

- **WHEN** the user drags a translate axis arrow
- **THEN** the selected entity moves along that axis
- **AND** the inspector position fields update live

### Requirement: Rotate gizmo shows interactive dials

In rotate mode with a selected entity, the viewport SHALL render thin rotation dials for X, Y, and Z axes and allow drag rotation. Dial tube radius MUST match Blender-proportioned thin rings while remaining pickable and visible.

#### Scenario: Rotation dials visible

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three thin rotation dials are drawn at the entity pivot
- **AND** at least one dial is clearly visible from a default perspective camera view

#### Scenario: Rotation dial drag updates entity

- **WHEN** the user drags a rotation dial
- **THEN** the selected entity rotates around the corresponding axis
- **AND** the inspector rotation fields update live

### Requirement: Scale gizmo is visible and interactive

In scale mode with a selected entity, the viewport SHALL render distinct scale handles as axis-end cubes (per-axis) and a uniform center cube, using `GizmoDrawStyle::scale_box`. Drag scaling behavior is unchanged.

#### Scenario: Scale handles visible

- **WHEN** scale mode is active and an entity is selected
- **THEN** per-axis cube handles and a uniform center cube are drawn at the pivot
- **AND** at least one scale handle is clearly visible from a default perspective camera view

#### Scenario: Axis scale drag updates entity

- **WHEN** the user drags a per-axis scale handle
- **THEN** the selected entity's scale changes along that axis only
- **AND** the inspector scale fields update live

#### Scenario: Uniform scale drag updates entity

- **WHEN** the user drags the uniform scale center handle
- **THEN** the selected entity's scale changes uniformly on all axes
- **AND** the inspector scale fields update live
