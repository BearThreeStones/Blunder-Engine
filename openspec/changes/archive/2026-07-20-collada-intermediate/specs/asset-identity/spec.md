## MODIFIED Requirements

### Requirement: Content root roles
The Project SHALL keep four content roles: Assets root for Intermediate descriptors (and Scene documents); Resources root (non-Source) for Intermediate data bodies; Source root (`Resources/Source/`) for Source Assets only; Cooked cache (`.blunder/cooked/`) for Final Assets keyed by GUID. Mesh Intermediate data bodies SHALL be COLLADA (`.dae`). Texture Intermediate data bodies SHALL remain image files. glTF/GLB SHALL NOT be registered as mesh Intermediate bodies after Import under this pipeline (they MAY appear temporarily as legacy Intermediate `source` until Intermediate Upgrade succeeds).

#### Scenario: Descriptor not stored under Resources
- **WHEN** Import creates a mesh or texture Asset
- **THEN** the YAML descriptor is written under the Assets root and Intermediate data under Resources (non-Source)

#### Scenario: Source archive under Source root
- **WHEN** Import performs Source Export from FBX, OBJ, glTF, or GLB
- **THEN** the original file is stored under the Source root and Intermediate COLLADA (`.dae`) under Resources (non-Source)

#### Scenario: Mesh Intermediate is COLLADA
- **WHEN** a mesh Asset has a successful modern Intermediate `source`
- **THEN** that `source` path refers to a `.dae` file under Resources (non-Source)
