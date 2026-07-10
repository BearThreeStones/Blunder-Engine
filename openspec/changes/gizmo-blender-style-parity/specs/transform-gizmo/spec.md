## MODIFIED Requirements

### Requirement: Gizmo theme colors match Blender default

Transform gizmo axis RGB SHALL match Blender default theme `TH_AXIS_X/Y/Z` from `userdef_default_theme.c`. View-align handles (`trans_c`, `rot_c`, `rot_t`, `scale_c_outer`) SHALL use white `TH_GIZMO_VIEW_ALIGN`.

#### Scenario: Default theme X axis color

- **WHEN** `gizmoAxisColor(trans_x, idot)` is queried with full visibility
- **THEN** RGB matches Blender `#FF3352` (normalized)
