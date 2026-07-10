## MODIFIED Requirements

### Requirement: Translate gizmo shows axis arrows and plane handles

In translate mode with a selected entity, the viewport SHALL render all three axis arrows (X, Y, Z) and three plane handles (XY, YZ, ZX) when not faded by view alignment, plus the center view-aligned handle. Axis arrows MUST use cone-tipped arrow geometry (stem + tip), not thick cross-billboard blocks. Arrow **stems** SHALL render as local 2D SDF line segments with Jacobian anti-aliasing (`kStrokeSdfSegment`), not screen-space polyline extrusion. Plane handles MUST use Blender-proportioned corner squares rendered as local 2D SDF rectangle borders (`kStrokeSdfRect`), not solid filled mesh quads. The center handle MUST use a smooth filled circular disc via SDF (`vsSdfRingQuad` + `discFillAlphaSolid`), not a polygon-fan tessellation.

#### Scenario: Axis arrows visible with plane handles

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** colored cone-tipped axis arrows extend from the gizmo pivot along each visible axis
- **AND** arrow stems show smooth anti-aliased edges along the stroke
- **AND** small plane handles are visible at the axis corners with smooth bordered square edges
- **AND** at least one axis arrow is visible from a default perspective camera view

#### Scenario: Translate axis drag moves entity

- **WHEN** the user drags a translate axis arrow
- **THEN** the selected entity moves along that axis
- **AND** the inspector position fields update live

#### Scenario: Center disc is smooth and pivot-locked

- **WHEN** translate mode is active and the user orbits the camera
- **THEN** the center disc remains view-aligned at the gizmo pivot
- **AND** the disc edge is smooth without visible polygon faceting

### Requirement: Scale gizmo is visible and interactive

In scale mode with a selected entity, the viewport SHALL render box+stem axis handles, plane corner handles, a uniform center scale cube, and a view-aligned outer annulus. Axis **stems** connecting the pivot to per-axis scale cubes SHALL render as local 2D SDF line segments with Jacobian anti-aliasing (`kStrokeSdfSegment`), matching translate arrow stems. Scale cube meshes remain solid geometry. Existing axis, plane, center, and uniform scale drag behavior is preserved or extended for plane handles.

#### Scenario: Scale handles visible

- **WHEN** scale mode is active and an entity is selected
- **THEN** box+stem axis handles, plane handles, center cube, and outer annulus are drawn at the pivot
- **AND** scale stems show smooth anti-aliased edges along the stroke
- **AND** at least one handle is clearly visible from a default perspective camera view

#### Scenario: Axis scale drag updates entity

- **WHEN** the user drags a per-axis scale handle
- **THEN** the selected entity's scale changes along that axis only
- **AND** the inspector scale fields update live

#### Scenario: Uniform scale drag updates entity

- **WHEN** the user drags the uniform scale center handle
- **THEN** the selected entity's scale changes uniformly on all axes
- **AND** the inspector scale fields update live

### Requirement: Transform gizmo matches Blender-style silhouettes

The transform gizmo overlay SHALL render Blender-like handle shapes in each active mode: translate uses cone-tipped axis arrows with small corner plane handles and a view-aligned center disc; rotate uses thin colored rings; scale uses axis-end cubes plus a uniform center cube. All styles MUST remain visible at default editor camera distances after styling changes. Translate center disc, arrow stems, plane borders, and scale stems MUST use the same local SDF anti-aliasing family as rotate wire rings.

#### Scenario: Move gizmo shows cone arrows and center disc

- **WHEN** translate mode is active and an entity is selected
- **AND** the camera is not aligned such that an axis is fully faded
- **THEN** each visible axis shows a thin stem with a cone tip at the positive axis end
- **AND** plane handles appear as small bordered squares at axis corners (not oversized blocks)
- **AND** the center handle is a white or neutral gray circular disc aligned to the view with smooth edges

#### Scenario: Rotate gizmo shows thin rings

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three colored rotation rings are drawn at the pivot
- **AND** ring tube thickness is visibly thinner than the prior visibility-fix thicken pass
- **AND** at least one ring is clearly visible from a default perspective camera view

#### Scenario: Scale gizmo shows axis-end cubes

- **WHEN** scale mode is active and an entity is selected
- **THEN** per-axis scale handles appear as small cubes at the positive end of each axis with smooth SDF stems
- **AND** the uniform scale handle appears as a small cube at the pivot center
- **AND** scale handles are not rendered as flat plane quads

### Requirement: Scale mode uses Blender box+stem axis handles

In scale mode, per-axis handles (X, Y, Z) SHALL render as box+stem arrows (`ED_GIZMO_ARROW_STYLE_BOX` with stem) rather than isolated cubes at axis tips. The stem portion MUST use local 2D SDF line segments with Jacobian anti-aliasing (`kStrokeSdfSegment`). Each handle MUST use the corresponding axis theme color.

#### Scenario: Scale axis handles show stem and box

- **WHEN** scale mode is active and an entity is selected
- **THEN** each visible axis shows a smooth anti-aliased stem line from the pivot region to a box handle at the positive axis end
- **AND** handles use red/green/blue axis colors respectively

### Requirement: Scale mode shows plane corner handles

In scale mode, the viewport SHALL render XY, YZ, and ZX plane handles as small corner squares with SDF-bordered edges (`ED_GIZMO_ARROW_STYLE_PLANE`) for two-axis scaling, matching Blender scale gizmo layout.

#### Scenario: Scale plane handles visible

- **WHEN** scale mode is active and an entity is selected
- **AND** the camera angle does not fully hide a plane handle pair
- **THEN** small bordered plane squares appear at the corners between visible scale axes

#### Scenario: Plane scale drag constrains two axes

- **WHEN** the user drags a scale plane handle (e.g. XY)
- **THEN** the entity scale changes on both corresponding axes only

## ADDED Requirements

### Requirement: Transform gizmo shader has no dead polyline stroke path

After Batch 2, `transform_gizmo.slang` SHALL NOT contain unused screen-space polyline extrusion (`emitPolyline`, `polylineExtrude`) or fragment smoothline handling for `strokeCoord ∈ [-1,1]`, because no gizmo draw style emits that stroke range.

#### Scenario: Polyline helpers removed

- **WHEN** the transform gizmo shader is compiled
- **THEN** `emitPolyline` and `polylineExtrude` are not present
- **AND** the fragment shader has no branch for `abs(strokeCoord) <= 1.0`

#### Scenario: SDF styles still render after cleanup

- **WHEN** translate, scale, and rotate gizmo modes are active
- **THEN** all handles using SDF stroke markers (`kStrokeSdfRing`, `kStrokeSdfDisc`, `kStrokeSdfSegment`, `kStrokeSdfRect`) render correctly
- **AND** `transform_gizmo_aa_test` and `gizmo_sdf_aa_test` pass
