## MODIFIED Requirements

### Requirement: Gizmo SDF AA CPU mirrors

The engine SHALL provide CPU functions in `gizmo_sdf_aa.{h,cpp}` that mirror local 2D SDF anti-aliasing math used by `transform_gizmo.slang` for filled discs, line segments, and axis-aligned rectangle borders, enabling unit tests without GPU.

#### Scenario: Disc fill alpha interior and exterior

- **WHEN** `discFillAlpha` is evaluated with signed pixel distance well inside the disc
- **THEN** alpha SHALL be 1.0
- **WHEN** `discFillAlpha` is evaluated with signed pixel distance well outside the disc
- **THEN** alpha SHALL be 0.0

#### Scenario: Segment distance on centerline

- **WHEN** `sdSegment2d` is evaluated at the midpoint of segment `[a,b]`
- **THEN** distance SHALL be 0.0
- **WHEN** `sdSegment2d` is evaluated 3 units perpendicular to the segment
- **THEN** distance SHALL be 3.0

#### Scenario: Segment stroke alpha on and off stroke

- **WHEN** `segmentLineAlpha` is evaluated on the segment centerline within half stroke width
- **THEN** alpha SHALL be 1.0
- **WHEN** `segmentLineAlpha` is evaluated far from the segment
- **THEN** alpha SHALL be 0.0

#### Scenario: Box distance inside and outside

- **WHEN** `sdBox2d` is evaluated at the center of an axis-aligned rectangle
- **THEN** signed distance SHALL be negative (inside)
- **WHEN** `sdBox2d` is evaluated 2 units beyond a corner along the diagonal
- **THEN** distance SHALL be approximately 2.0

#### Scenario: Rectangle border alpha on and off edge

- **WHEN** `rectRingAlpha` is evaluated on the rectangle edge within half stroke width
- **THEN** alpha SHALL be 1.0
- **WHEN** `rectRingAlpha` is evaluated far from the rectangle border
- **THEN** alpha SHALL be 0.0

#### Scenario: CPU regression test passes

- **WHEN** `gizmo_sdf_aa_test` runs in CI
- **THEN** all assertions SHALL pass
