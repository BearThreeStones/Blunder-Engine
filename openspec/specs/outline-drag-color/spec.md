# outline-drag-color

## Purpose

Transform-gizmo drag state drives outline resolve color (object-select vs transform orange).

## Requirements

### Requirement: Transform-drag outline color

While the transform gizmo is actively dragging the selection (`TransformGizmoController::isDragging()` is true), the outline resolve pass SHALL composite pixels using the transform-drag orange color instead of the object-select orange color.

#### Scenario: Begin gizmo drag

- **WHEN** the user starts dragging a translate, rotate, or scale gizmo handle on a selected mesh entity
- **THEN** the viewport outline color SHALL switch to the hardcoded transform orange for the duration of the drag

#### Scenario: End gizmo drag

- **WHEN** the user releases the mouse button and the gizmo drag ends
- **THEN** the viewport outline color SHALL return to the object-select orange (`#F57011`)

#### Scenario: No selection during drag

- **WHEN** selection is cleared while a gizmo drag is active
- **THEN** the outline pass SHALL be disabled and no outline pixels SHALL be drawn
