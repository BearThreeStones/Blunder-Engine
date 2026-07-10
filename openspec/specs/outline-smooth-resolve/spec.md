# outline-smooth-resolve

## Purpose

Sub-pixel smoothstep edge coverage for anti-aliased selection outlines.

## Requirements

### Requirement: Smooth anti-aliased outline edges

The outline resolve pass SHALL anti-alias silhouette edges using a sub-pixel edge coverage calculation (smoothstep-based neighbor filtering), producing visually smooth outline strokes without a separate UI toggle.

#### Scenario: Diagonal silhouette edge

- **WHEN** a selected mesh presents a diagonal silhouette edge in screen space
- **THEN** the outline SHALL render with partial-alpha pixels along the edge (not a hard aliased 1-pixel stair-step only)

#### Scenario: Default enabled

- **WHEN** the editor renders a frame with an active selection outline
- **THEN** smooth outline resolve SHALL be active without requiring environment variables or user preferences

#### Scenario: Depth occlusion preserved

- **WHEN** a smooth outline edge pixel is occluded by closer scene geometry
- **THEN** the pixel SHALL still apply the reduced occlusion alpha factor (`0.35`) in addition to smooth edge coverage
