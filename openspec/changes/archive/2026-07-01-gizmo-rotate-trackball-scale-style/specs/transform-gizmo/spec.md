## ADDED Requirements

### Requirement: Rotate mode shows trackball background overlay

In rotate mode with a selected entity, the viewport SHALL render a view-aligned filled disc at the gizmo pivot representing the Blender trackball (`MAN_AXIS_ROT_T`). The overlay MUST use `TH_GIZMO_VIEW_ALIGN` color at approximately 5% of normal gizmo alpha and MUST NOT use semicircle clip plane clipping.

#### Scenario: Trackball disc visible beneath dials

- **WHEN** rotate mode is active and an entity is selected
- **THEN** a subtle filled view-aligned disc is visible at the pivot beneath colored axis dials and the outer white ring
- **AND** the disc alpha is visibly lower than axis dial or outer ring opacity

#### Scenario: Trackball follows camera

- **WHEN** the user orbits the camera in rotate mode
- **THEN** the trackball disc remains aligned to the view plane at the pivot

### Requirement: Scale mode uses Blender box+stem axis handles

In scale mode, per-axis handles (X, Y, Z) SHALL render as box+stem arrows (`ED_GIZMO_ARROW_STYLE_BOX` with stem) rather than isolated cubes at axis tips. Each handle MUST use the corresponding axis theme color.

#### Scenario: Scale axis handles show stem and box

- **WHEN** scale mode is active and an entity is selected
- **THEN** each visible axis shows a stem line from the pivot region to a box handle at the positive axis end
- **AND** handles use red/green/blue axis colors respectively

### Requirement: Scale mode shows plane corner handles

In scale mode, the viewport SHALL render XY, YZ, and ZX plane handles as small corner squares (`ED_GIZMO_ARROW_STYLE_PLANE`) for two-axis scaling, matching Blender scale gizmo layout.

#### Scenario: Scale plane handles visible

- **WHEN** scale mode is active and an entity is selected
- **AND** the camera angle does not fully hide a plane handle pair
- **THEN** small plane squares appear at the corners between visible scale axes

#### Scenario: Plane scale drag constrains two axes

- **WHEN** the user drags a scale plane handle (e.g. XY)
- **THEN** the entity scale changes on both corresponding axes only

## MODIFIED Requirements

### Requirement: Rotate gizmo shows interactive dials

In rotate mode with a selected entity, the viewport SHALL render rotation dials for X, Y, and Z axes, a view-aligned outer white ring, a trackball background overlay, and allow drag rotation on axis dials. Axis dials use semicircle clipping when idle; outer ring and trackball do not.

#### Scenario: Rotation dials visible

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three colored rotation dials, one outer white ring, and one trackball overlay are drawn at the entity pivot
- **AND** at least one element is clearly visible from a default perspective camera view

#### Scenario: Rotation dial drag updates entity

- **WHEN** the user drags a rotation dial
- **THEN** the selected entity rotates around the corresponding axis
- **AND** the inspector rotation fields update live

### Requirement: Scale gizmo is visible and interactive

In scale mode with a selected entity, the viewport SHALL render box+stem axis handles, plane corner handles, a uniform center scale cube, and a view-aligned outer annulus. Existing axis, plane, center, and uniform scale drag behavior is preserved or extended for plane handles.

#### Scenario: Scale handles visible

- **WHEN** scale mode is active and an entity is selected
- **THEN** box+stem axis handles, plane handles, center cube, and outer annulus are drawn at the pivot
- **AND** at least one handle is clearly visible from a default perspective camera view

#### Scenario: Axis scale drag updates entity

- **WHEN** the user drags a per-axis scale handle
- **THEN** the selected entity's scale changes along that axis only
- **AND** the inspector scale fields update live

#### Scenario: Uniform scale drag updates entity

- **WHEN** the user drags the uniform scale center handle
- **THEN** the selected entity's scale changes uniformly on all axes
- **AND** the inspector scale fields update live
