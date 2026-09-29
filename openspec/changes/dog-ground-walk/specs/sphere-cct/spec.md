## Requirement: Character Controller sphere mode

The Character Controller Unique SHALL support a `sphere` shape mode in addition to the default `capsule`. Sphere mode uses `radius` and optional local `offset`; capsule `height` is unused for the sweep. Move-and-slide SHALL sweep with `PhysicsSweepShape::Sphere` when sphere mode is set.

### Scenario: Authored sphere CCT

- **WHEN** a scene entity has `characterController.shape` = `sphere` with radius 0.7 and a local offset
- **THEN** Play `MoveAndSlide` casts a sphere of that radius centred at entity origin plus offset

## Requirement: Sphere × static trimesh continuous sweep

`PhysicsWorld::shapecast` for a sphere query against a static triangle mesh SHALL use continuous inflated-mesh casting (equivalent reliability to capsule endpoint inflated casts), not only a centre point ray plus end discrete overlap.

### Scenario: Thin plate

- **WHEN** a sphere of radius 0.7 shapecasts downward onto a thin authored triangle plate
- **THEN** the cast reports a hit before tunnelling through the plate
