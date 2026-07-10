## MODIFIED Requirements

### Requirement: Smooth anti-aliased outline edges

The outline resolve pass SHALL anti-alias silhouette edges using a sub-pixel edge coverage calculation on an 8-neighbor silhouette mask with smoothstep parameters `kOutlineEdgeSmoothMin = 0.25` and `kOutlineEdgeSmoothMax = 2.5`, producing visually smooth outline strokes without a separate UI toggle. A CPU-testable mirror of the coverage function SHALL exist in `outline_aa.cpp`.

#### Scenario: Diagonal silhouette edge

- **WHEN** a selected mesh presents a diagonal silhouette edge in screen space
- **THEN** the outline SHALL render with partial-alpha pixels along the edge (not a hard aliased 1-pixel stair-step only)

#### Scenario: Shallow diagonal partial coverage

- **WHEN** a silhouette pixel has 2–3 edge neighbors on the 8-neighbor mask
- **THEN** outline coverage SHALL be strictly between 0 and 1 (partial alpha)

#### Scenario: Default enabled

- **WHEN** the editor renders a frame with an active selection outline
- **THEN** smooth outline resolve SHALL be active without requiring environment variables or user preferences

#### Scenario: Depth occlusion preserved

- **WHEN** a smooth outline edge pixel is occluded by closer scene geometry
- **THEN** the pixel SHALL still apply the reduced occlusion alpha factor (`0.35`) in addition to smooth edge coverage
