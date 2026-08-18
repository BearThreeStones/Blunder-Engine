## ADDED Requirements

### Requirement: Placement Preview motion forces viewport redraw
While Placement Preview is visible and Ground placement updates with pointer motion, the editor SHALL request a viewport redraw so the follow-mesh tracks under a static Editor Camera.

#### Scenario: Follow-mesh under static camera
- **WHEN** Placement Preview is visible and the pointer moves over the viewport
- **AND** the editor camera matrices do not change
- **THEN** the preview mesh SHALL move to the new Ground placement on subsequent frames
