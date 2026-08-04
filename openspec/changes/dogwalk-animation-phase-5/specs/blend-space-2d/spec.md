## ADDED Requirements

### Requirement: BlendSpace2D triangulation blend
AnimationTree SHALL provide **BlendSpace2D** nodes that blend among authored clip points on a 2D parameter plane. Runtime SHALL find a triangle and blend up to three neighbors with **barycentric** weights using local TRS lerp / rotation slerp. Scripts SHALL set the 2D parameter by **node logical name**. Phase 5 SHALL NOT require a regular grid as the only authorship layout.

#### Scenario: Interior point barycentric blend
- **WHEN** a BlendSpace2D scalar `(x,y)` lies inside a triangle of three clip points
- **THEN** the base pose is the barycentric-weighted local TRS blend of those three sampled clips

#### Scenario: Per-node 2D parameter
- **WHEN** a script sets BlendSpace2D parameters on node logical name `Locomotion2D`
- **THEN** only that node's 2D parameter updates
