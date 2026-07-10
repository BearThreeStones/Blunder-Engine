## ADDED Requirements

### Requirement: Rotate dials clip to camera-facing semicircle when idle

When rotate mode is active, the user is not dragging a rotation handle, and a rotation dial is drawn with `GizmoDrawStyle::dial`, the viewport SHALL clip each dial to the half facing the camera using a view-aligned clip plane through the gizmo pivot. The back-facing half of each ring MUST NOT be visible.

#### Scenario: Idle rotate mode shows semicircular dials

- **WHEN** rotate mode is active and an entity is selected
- **AND** the user is not dragging a rotation dial
- **THEN** each visible rotation dial appears as a semicircle facing the camera
- **AND** the portion of the ring behind the clip plane is not drawn

#### Scenario: Clip plane follows camera

- **WHEN** the user orbits the camera around a selected entity in rotate mode
- **THEN** the visible semicircle of each dial updates to remain camera-facing

### Requirement: Full rotation dial during active drag

While the user is actively dragging a rotation dial, clip plane clipping MUST be disabled for rotation dial draws so the manipulated ring appears as a full circle (Blender modal behavior).

#### Scenario: Dragging shows full dial ring

- **WHEN** the user begins dragging a rotation dial
- **THEN** the rotation dials render without half-arc clip plane clipping
- **AND** the ghost arc overlay continues to show angular drag feedback separately

#### Scenario: Clip resumes after drag release

- **WHEN** the user releases the mouse after a rotation drag
- **THEN** rotation dials return to camera-facing semicircle rendering

## MODIFIED Requirements

### Requirement: Rotate gizmo shows interactive dials

In rotate mode with a selected entity, the viewport SHALL render rotation dials for X, Y, and Z axes and allow drag rotation. Idle dials MUST use camera-facing semicircle clipping; dials during an active rotation drag MUST render as full circles without semicircle clipping.

#### Scenario: Rotation dials visible

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three rotation dials are drawn at the entity pivot
- **AND** at least one dial semicircle is clearly visible from a default perspective camera view

#### Scenario: Rotation dial drag updates entity

- **WHEN** the user drags a rotation dial
- **THEN** the selected entity rotates around the corresponding axis
- **AND** the inspector rotation fields update live
