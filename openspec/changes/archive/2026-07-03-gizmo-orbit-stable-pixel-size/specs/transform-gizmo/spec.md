## MODIFIED Requirements

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
