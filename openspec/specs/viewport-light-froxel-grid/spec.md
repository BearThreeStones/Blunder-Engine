# viewport-light-froxel-grid Specification

## Purpose

Build, once per rendered frame, a GPU-resident 3D grid of screen-tile-by-depth-slice cells ("froxels") over the editor Viewport and assign visible point and spot lights into every froxel they overlap, so downstream shading can bound its per-pixel light list without a global light cap.

## Requirements

### Requirement: Froxel grid is a 3D screen-tile-by-depth-slice grid
The froxel grid SHALL be a 3D grid: XY cells are screen-space tiles and Z cells are view-depth slices. The system SHALL NOT implement this as a 2D-tile-only grid (no depth subdivision) and SHALL NOT fill it on the CPU.

#### Scenario: Grid has depth subdivision
- **WHEN** the Viewport builds the froxel grid for a frame
- **THEN** the grid has more than one Z slice at every XY tile

### Requirement: Default tile size and Z slice count
The froxel grid's default XY tile size SHALL be **64 pixels** and its default Z slice count SHALL be **32**, matching the Unreal Forward Light Grid defaults this slice is modeled on.

#### Scenario: Default grid dimensions
- **WHEN** the Viewport is at a resolution not otherwise configured for a different tile size or slice count
- **THEN** the froxel grid uses 64-pixel screen tiles and 32 Z slices

### Requirement: Z slices are exponentially distributed
The mapping from view depth to Z slice index SHALL be exponential (denser slices near the camera, sparser slices toward the far plane), not linear.

#### Scenario: Near slices are thinner than far slices
- **WHEN** two Z slices are compared, one adjacent to the near plane and one adjacent to the far plane
- **THEN** the near-plane slice spans a smaller view-depth range than the far-plane slice

### Requirement: Froxel fill runs as one GPU compute pass per frame
The system SHALL fill the froxel grid's light assignments using one GPU compute pass per rendered frame. The system SHALL NOT assign lights to froxels on the CPU.

#### Scenario: Grid reflects current-frame light state
- **WHEN** a point or spot Light Component's pose or range changes between two consecutive rendered frames
- **THEN** the froxel grid built for the second frame reflects the changed pose or range

### Requirement: Only point and spot lights are assigned into froxels
The froxel fill pass SHALL assign visible, Light-enabled Point Light and Spot Light components into every froxel their influence overlaps. It SHALL NOT assign Directional Light or Area Light components into any froxel.

#### Scenario: Directional light is never in a froxel list
- **WHEN** the open scene has a Light-enabled Directional Light and at least one froxel
- **THEN** no froxel's assigned-light list contains that Directional Light

#### Scenario: Point light appears in every froxel it overlaps
- **WHEN** a Light-enabled Point Light's range overlaps two adjacent froxels
- **THEN** the froxel fill pass assigns that Point Light into both froxels

### Requirement: Per-froxel light cap with recorded overflow
Each froxel SHALL hold at most **64** assigned lights. When more than 64 lights overlap a single froxel, the froxel fill pass SHALL keep 64 and drop the remainder for that froxel only; it SHALL NOT grow that froxel's list past 64 and SHALL NOT use an unbounded or linked-list-based per-froxel structure this slice. Each drop SHALL be recorded through a log message and a Viewport stat counter.

#### Scenario: 65th overlapping light is dropped for that froxel
- **WHEN** 65 Light-enabled Point Lights all overlap the same froxel
- **THEN** that froxel's assigned-light list contains 64 of them
- **AND** a log message and the Viewport's dropped-light stat record the 65th

#### Scenario: Overflow in one froxel does not affect another
- **WHEN** one froxel has more than 64 overlapping lights and an adjacent froxel has fewer than 64
- **THEN** the adjacent froxel's assigned-light list is unaffected by the first froxel's overflow
