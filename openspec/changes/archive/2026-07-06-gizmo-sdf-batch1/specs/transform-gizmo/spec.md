## MODIFIED Requirements

### Requirement: Translate gizmo shows axis arrows and plane handles

In translate mode with a selected entity, the viewport SHALL render all three axis arrows (X, Y, Z) and three plane handles (XY, YZ, ZX) when not faded by view alignment, plus the center view-aligned handle. Axis arrows MUST use cone-tipped arrow geometry (stem + tip), not thick cross-billboard blocks. Arrow **stems** SHALL render as local 2D SDF line segments with Jacobian anti-aliasing (`kStrokeSdfSegment`), not screen-space polyline extrusion. Plane handles MUST use Blender-proportioned corner squares. The center handle MUST use a smooth filled circular disc via SDF (`vsSdfRingQuad` + `discFillAlphaSolid`), not a polygon-fan tessellation.

#### Scenario: Axis arrows visible with plane handles

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** colored cone-tipped axis arrows extend from the gizmo pivot along each visible axis
- **AND** arrow stems show smooth anti-aliased edges along the stroke
- **AND** small plane handles are visible at the axis corners
- **AND** at least one axis arrow is visible from a default perspective camera view

#### Scenario: Translate axis drag moves entity

- **WHEN** the user drags a translate axis arrow
- **THEN** the selected entity moves along that axis
- **AND** the inspector position fields update live

#### Scenario: Center disc is smooth and pivot-locked

- **WHEN** translate mode is active and the user orbits the camera
- **THEN** the center disc remains centered on the gizmo pivot
- **AND** the disc edge is smooth (no visible polygon facets)

### Requirement: Transform gizmo matches Blender-style silhouettes

The transform gizmo overlay SHALL render Blender-like handle shapes in each active mode: translate uses cone-tipped axis arrows with small corner plane handles and a view-aligned center disc; rotate uses thin colored rings; scale uses axis-end cubes plus a uniform center cube. All styles MUST remain visible at default editor camera distances after styling changes. Translate center disc and arrow stems MUST use the same local SDF anti-aliasing family as rotate wire rings.

#### Scenario: Move gizmo shows cone arrows and center disc

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** each visible axis shows a thin stem with a cone tip at the positive axis end
- **AND** plane handles appear as small squares at axis corners (not oversized blocks)
- **AND** the center handle is a white or neutral gray circular disc aligned to the view with smooth edges

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

## REMOVED Requirements

### Requirement: Transform gizmo arrow stem polylines

**Reason**: Arrow stems now use local SDF segment rendering (`kStrokeSdfSegment`) for consistent 3D anchoring and shared AA with rotate rings. Screen-space polyline extrusion caused coordinate-space divergence from SDF rings.

**Migration**: Visual QA in translate mode; `gizmo_sdf_aa_test` and `transform_gizmo_hover_test` replace polyline-stem-specific AA checks.
