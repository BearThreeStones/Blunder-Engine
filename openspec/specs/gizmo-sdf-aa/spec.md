# gizmo-sdf-aa Specification

## Purpose

CPU mirrors of local 2D SDF anti-aliasing math used by transform gizmo shaders, enabling unit tests without GPU.

## Requirements

### Requirement: Gizmo SDF AA CPU mirrors

The engine SHALL provide CPU functions in `gizmo_sdf_aa.{h,cpp}` that mirror local 2D SDF anti-aliasing math used by `transform_gizmo.slang` for filled discs and line segments, enabling unit tests without GPU.

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

#### Scenario: CPU regression test passes

- **WHEN** `gizmo_sdf_aa_test` runs in CI
- **THEN** all assertions SHALL pass
