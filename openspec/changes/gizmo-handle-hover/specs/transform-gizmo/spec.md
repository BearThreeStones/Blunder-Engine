## ADDED Requirements

### Requirement: Gizmo handle hover highlight

When a transform gizmo mode (translate, rotate, or scale) is active, the user is not dragging, and the cursor is over an interactive handle, the viewport SHALL render that handle with highlight color (`gizmoAxisColor().color_hi`). Hover highlight MUST use the same CPU analytic pick geometry as click/drag picking. Hover MUST NOT change scene selection or block viewport mesh picking.

#### Scenario: Hover highlights handle under cursor

- **WHEN** translate, rotate, or scale mode is active
- **AND** the user is not dragging a gizmo handle
- **AND** the mouse cursor is over a visible interactive handle
- **THEN** that handle renders with `color_hi` until the cursor leaves the handle

#### Scenario: Hover clears when cursor leaves handle

- **WHEN** the cursor moves off the currently hovered handle
- **THEN** that handle returns to normal color on the next viewport frame

#### Scenario: Drag highlight takes precedence

- **WHEN** the user is dragging a gizmo handle
- **THEN** only the active dragged handle is highlighted
- **AND** hover highlight is suppressed until drag ends

#### Scenario: Hover does not change selection

- **WHEN** the user moves the mouse over a gizmo handle without clicking
- **THEN** the current editor selection remains unchanged
- **AND** viewport mesh picking behavior on subsequent click is unchanged

#### Scenario: Hover uses Blender alpha parity

- **WHEN** an interactive axis handle is hovered
- **THEN** its color RGB matches the normal state
- **AND** its alpha is `k_gizmo_color_alpha_hi` (1.0) multiplied by the same `alpha_fac` as `gizmo_get_axis_color`
- **AND** non-interactive decor (trackball, outer ring, scale annulus) do not switch to `color_hi` on hover
