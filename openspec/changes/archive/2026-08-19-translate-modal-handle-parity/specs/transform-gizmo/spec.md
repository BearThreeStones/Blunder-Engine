## MODIFIED Requirements

### Requirement: Translate gizmo shows axis arrows and plane handles

In translate mode with a selected entity, the viewport SHALL render all three axis arrows (X, Y, Z) and three plane handles (XY, YZ, ZX) when not faded by view alignment, plus the center view-aligned handle. Axis arrows MUST use cone-tipped arrow geometry (stem + tip), not thick cross-billboard blocks. Arrow **stems** SHALL render as local 2D SDF line segments with Jacobian anti-aliasing (`kStrokeSdfSegment`), not screen-space polyline extrusion. Plane handles MUST use Blender-proportioned corner squares. The center handle MUST use a smooth filled circular disc via SDF (`vsSdfRingQuad` + `discFillAlphaSolid`), not a polygon-fan tessellation. Translate handle dragging SHALL use screen-space mouse motion mapped through a Translate Modal Session (axis / infinite-plane / free-center constraints), not finite plane-quad ray tracking.

#### Scenario: Axis arrows visible with plane handles

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** colored cone-tipped axis arrows extend from the gizmo pivot along each visible axis
- **AND** arrow stems show smooth anti-aliased edges along the stroke
- **AND** small plane handles are visible at the axis corners
- **AND** at least one axis arrow is visible from a default perspective camera view

#### Scenario: Translate axis drag moves entity

- **WHEN** the user drags a translate axis arrow
- **THEN** the selected entity moves along that axis using screen-space motion constrained to the axis
- **AND** the inspector position fields update live
- **AND** motion continues when the pointer leaves the on-screen arrow geometry

#### Scenario: Translate plane drag uses infinite plane

- **WHEN** the user drags a translate plane handle
- **THEN** the selected entity moves in that plane using screen-space motion on an infinite constraint plane
- **AND** the inspector position fields update live

#### Scenario: Center disc is smooth and pivot-locked

- **WHEN** translate mode is active and the user orbits the camera
- **THEN** the center disc remains centered on the gizmo pivot
- **AND** the disc edge is smooth (no visible polygon facets)
