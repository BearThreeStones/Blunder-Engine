## ADDED Requirements

### Requirement: Transform edits force viewport redraw

When the user changes a selected entity's transform via gizmo drag or Inspector TRS fields, the editor SHALL request a viewport redraw so mesh and gizmo overlays update without camera motion.

#### Scenario: Gizmo translate drag under static camera

- **WHEN** a selected entity is dragged with the translate gizmo
- **AND** the editor camera matrices do not change
- **THEN** the viewport mesh and gizmo handles SHALL move with the drag on subsequent frames

#### Scenario: Inspector position edit under static camera

- **WHEN** the user edits Inspector position/rotation/scale for the selection
- **AND** the editor camera matrices do not change
- **THEN** the viewport mesh and gizmo handles SHALL update to the new transform without requiring a camera move
