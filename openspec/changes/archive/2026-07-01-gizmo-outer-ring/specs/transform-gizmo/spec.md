## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Rotate gizmo shows interactive dials

In rotate mode with a selected entity, the viewport SHALL render rotation dials for X, Y, and Z axes, a view-aligned outer white ring, and allow drag rotation on axis dials. Axis dials use semicircle clipping when idle; the outer ring does not.

#### Scenario: Rotation dials visible

- **WHEN** rotate mode is active and an entity is selected
- **THEN** three colored rotation dials and one view-aligned outer ring are drawn at the entity pivot
- **AND** at least one dial semicircle or the outer ring is clearly visible from a default perspective camera view

#### Scenario: Rotation dial drag updates entity

- **WHEN** the user drags a rotation dial
- **THEN** the selected entity rotates around the corresponding axis
- **AND** the inspector rotation fields update live

### Requirement: Scale gizmo is visible and interactive

In scale mode with a selected entity, the viewport SHALL render per-axis scale cubes, a uniform center scale cube, and a view-aligned outer annulus. Existing axis and center scale drag behavior is unchanged.

#### Scenario: Scale handles visible

- **WHEN** scale mode is active and an entity is selected
- **THEN** per-axis scale cubes, a uniform center cube, and the outer annulus are drawn at the pivot
- **AND** at least one scale handle or the annulus is clearly visible from a default perspective camera view

#### Scenario: Axis scale drag updates entity

- **WHEN** the user drags a per-axis scale handle
- **THEN** the selected entity's scale changes along that axis only
- **AND** the inspector scale fields update live

#### Scenario: Uniform scale drag updates entity

- **WHEN** the user drags the uniform scale center handle
- **THEN** the selected entity's scale changes uniformly on all axes
- **AND** the inspector scale fields update live
