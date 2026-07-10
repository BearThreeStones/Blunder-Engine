## ADDED Requirements

### Requirement: Gizmo scale matches Blender wm_gizmo_calculate_scale

The engine SHALL compute transform gizmo world scale using the Blender model: `scale_final = scale_basis × ui_scale × gizmo_size × pixel_size_no_ui_scale(pivot)`, where `pixel_size_no_ui_scale` derives from viewport `pixsize` and, in perspective mode only, multiplies by pivot depth. In orthographic mode, gizmo screen size MUST NOT depend on pivot depth.

#### Scenario: Perspective gizmo constant screen size when orbiting distance

- **WHEN** the user zooms the perspective camera in or out while keeping the same viewport size
- **THEN** the transform gizmo maintains approximately the same screen pixel diameter at the selection pivot

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
