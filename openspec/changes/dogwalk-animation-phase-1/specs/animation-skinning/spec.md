## ADDED Requirements

### Requirement: Fast Path uses CPU skinning
When a skinned Mesh Asset is shown via Fast Path (Intermediate, Final missing or stale), the runtime SHALL deform vertices on the CPU from Skeleton pose and bind weights, then draw the deformed mesh through the existing mesh path as applicable.

#### Scenario: Uncooked skinned preview
- **WHEN** a skinned character is visible in the editor and Final is missing or stale
- **THEN** the mesh deforms with CPU skinning from the current Skeleton pose

### Requirement: Final uses GPU skinning
When a skinned Mesh Asset is loaded from a fresh Final, the runtime SHALL deform using GPU skinning (bone palette / skinning shader). Editor sessions with a fresh Final and the Player SHALL share this Final/GPU path.

#### Scenario: Cooked skinned draw
- **WHEN** Final for a skinned mesh is fresh and used for draw
- **THEN** deformation uses GPU skinning, not CPU Fast Path as the primary path

#### Scenario: Editor Final matches Player path
- **WHEN** the editor displays a skinned mesh from fresh Final and Player loads the same Final
- **THEN** both use the GPU skinned draw path

### Requirement: Bind data from Intermediate glTF
Skinned mesh bind pose, inverse bind matrices, and vertex joint weights SHALL be obtained from mesh Intermediate glTF/GLB (and Cooked into Final). AnimationClip Assets SHALL NOT be the sole carrier of bind weights.

#### Scenario: Skin without clip still binds
- **WHEN** a skinned Mesh Asset is loaded at rest with no clip playing
- **THEN** the mesh can render in bind/rest pose using Intermediate/Final skin data
