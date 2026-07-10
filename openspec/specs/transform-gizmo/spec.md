# transform-gizmo Specification

## Purpose
TBD - created by archiving change fix-transform-gizmo. Update Purpose after archive.
## Requirements
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

### Requirement: Transform toolbar and shortcuts reflect engine mode

The Slint transform toolbar and keyboard shortcuts (G, R, S) SHALL stay synchronized with `TransformGizmoController` mode without overwriting user mode on selection change.

#### Scenario: Toolbar shows active mode after selection change

- **WHEN** the user is in Scale mode and selects a different entity
- **THEN** the toolbar Scale button remains highlighted

#### Scenario: Hotkey sets mode

- **WHEN** the user presses R with a selected entity
- **THEN** the gizmo switches to rotate mode
- **AND** the toolbar shows Rotate as active

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

### Requirement: Rotate mode shows view-aligned outer white ring

In rotate mode with a selected entity, the viewport SHALL render a view-aligned outer wire ring at the gizmo pivot using `TH_GIZMO_VIEW_ALIGN` white/gray color. The outer ring MUST be a full 360° circle (no semicircle clip plane). Its visual size MUST be larger than the colored X/Y/Z axis dials (Blender scale basis 1.2×).

#### Scenario: Outer ring visible in rotate mode

- **WHEN** rotate mode is active and an entity is selected
- **THEN** a white or neutral gray circular wire ring is drawn aligned to the camera view plane at the pivot
- **AND** the ring appears larger than the colored axis rotation dials
- **AND** the ring is not clipped to a semicircle while idle

#### Scenario: Outer ring uses view-align color

- **WHEN** the rotate outer ring is drawn
- **THEN** its color matches the engine's view-align gizmo theme color (same family as translate center disc)

### Requirement: Scale mode shows view-aligned outer white annulus

In scale mode with a selected entity, the viewport SHALL render a view-aligned outer annulus (two concentric wire circles) at the gizmo pivot using `TH_GIZMO_VIEW_ALIGN` color. The annulus MUST have an outer circle and a smaller inner circle (Blender `ED_GIZMO_PRIMITIVE_STYLE_ANNULUS` with `arc_inner_factor` 6.0).

#### Scenario: Annulus visible in scale mode

- **WHEN** scale mode is active and an entity is selected
- **THEN** two concentric white or neutral gray wire circles are drawn aligned to the camera view plane at the pivot
- **AND** the inner circle radius is visibly smaller than the outer circle radius

#### Scenario: Annulus surrounds center scale handle

- **WHEN** scale mode is active with both annulus and center uniform scale cube visible
- **THEN** the annulus is drawn around the center scale handle without obscuring axis-end scale cubes

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

### Requirement: Gizmo scale matches Blender wm_gizmo_calculate_scale

The engine SHALL compute transform gizmo world scale using: `scale_final = scale_basis × ui_scale × gizmo_size × pixel_size_no_ui_scale(pivot)`, where `pixel_size_no_ui_scale` derives from viewport `pixsize` and, in perspective mode only, multiplies by pivot depth (`zfac` / `clip.w` from full view×projection). Viewport `pixsize` SHALL be derived from the **projection matrix only** (not view×projection), so perspective pixel size at a fixed pivot on the view axis remains stable when the camera orbits without zoom.

In orthographic mode, gizmo screen size MUST NOT depend on pivot depth.

#### Scenario: Perspective gizmo constant screen size when orbiting distance

- **WHEN** the user zooms the perspective camera in or out while keeping the same viewport size
- **THEN** the transform gizmo maintains approximately the same screen pixel diameter at the selection pivot

#### Scenario: Perspective gizmo stable size during camera orbit

- **WHEN** the user right-click orbits the perspective camera (yaw/pitch change) without zooming
- **AND** the gizmo pivot remains at the viewport center (camera focal point)
- **THEN** the transform gizmo uniform scale (`group_scale`) varies by less than 5% compared to any other orbit angle at the same distance

#### Scenario: Persp/Iso toggle preserves gizmo screen size

- **WHEN** the user toggles between perspective and orthographic projection at the same orbit distance (matching ortho_size)
- **THEN** the transform gizmo screen diameter before and after toggle differs by less than 15%

#### Scenario: Orthographic gizmo depth-independent

- **WHEN** the viewport is in orthographic mode and the selection pivot moves along the view axis
- **THEN** the gizmo screen size remains unchanged

### Requirement: Rotation dials use screen-space polyline rendering

Rotation axis dials (`rot_x`, `rot_y`, `rot_z`), the view-aligned outer ring (`rot_c`), and the scale outer annulus wire ring SHALL render as screen-space polylines with fixed pixel line width, matching Blender dial gizmo behavior. Tube geometry MUST NOT be used for these wire rings.

#### Scenario: Rotation dial line width in pixels

- **WHEN** rotate mode is active and the viewport is in perspective at default zoom
- **THEN** X/Y/Z rotation dial strokes render at approximately 3 screen pixels wide
- **AND** the outer white ring renders at approximately 2 screen pixels wide

#### Scenario: Dial radius scales, stroke width does not

- **WHEN** the user changes viewport size or toggles projection mode
- **THEN** dial ring radius in screen pixels tracks `gizmo_size` (approximately 75px diameter baseline)
- **AND** dial stroke width in screen pixels remains approximately constant (3px / 2px)

#### Scenario: Semicircle clip still applies to axis dials

- **WHEN** rotate mode is idle (not dragging)
- **THEN** axis rotation dials still use view-aligned clip plane semicircle hiding
- **AND** polyline rendering respects the same clip plane discard as before

### Requirement: Small viewport gizmo size cap

When the viewport is smaller than a threshold, the engine SHALL reduce effective `gizmo_size` so the transform gizmo does not exceed approximately 35% of the smaller viewport dimension, consistent with the navigate gizmo layout policy.

#### Scenario: Docked small viewport

- **WHEN** the 3D viewport height is 300px or less in perspective mode
- **THEN** the transform gizmo screen diameter is less than 40% of the viewport height

### Requirement: Scale and translate gizmo behavior preserved

Changes to scale formula and polyline dials MUST NOT regress translate arrow, scale stem/box, plane handle, center cube, trackball fill, or dial ghost arc rendering and interaction.

#### Scenario: Translate and scale modes unchanged after scale fix

- **WHEN** the user switches to translate or scale mode after this change
- **THEN** handles remain visible, pickable, and draggable as before

