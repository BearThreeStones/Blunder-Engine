## ADDED Requirements

### Requirement: Navigate gizmo Blender visual parity

The viewport navigate orientation gizmo SHALL match Blender `VIEW3D_GT_navigate_rotate`: axis RGB from `TH_AXIS_*`, depth-based fade against view background, diameter 80px at UI scale 1.0, offset 10px from top-right.

#### Scenario: Default navigate diameter

- **WHEN** viewport is 1920×1080
- **THEN** `computeNavigateGizmoLayout` gizmo size is 80 logical pixels

#### Scenario: Depth fade

- **WHEN** a positive axis endpoint faces the camera (depth ≈ 1)
- **THEN** its color is brighter than when facing away (depth ≈ -1)

### Requirement: Navigate gizmo hover highlight

When the cursor is inside the navigate gizmo bounds, the gizmo SHALL show Blender-style hover feedback.

#### Scenario: Hover shows highlight backdrop

- **WHEN** the cursor is inside the navigate gizmo widget
- **THEN** a circular backdrop using gray `color_hi` (0.5 RGB, 0.5 alpha) replaces the idle disc
- **AND** the axis under the cursor uses emphasized label styling
