# asset-identity Specification

## Purpose
TBD - created by archiving change asset-pipeline-pull. Update Purpose after archive.
## Requirements
### Requirement: Asset identity is GUID
An Asset SHALL be identified by a GUID. The Asset Descriptor under the Assets root SHALL persist that GUID. The Asset Registry SHALL map GUID to descriptor virtual path. Product operations (reference, Cook, Content Browser identity) SHALL address Assets by GUID, not by filesystem path alone.

#### Scenario: Registry resolves GUID to descriptor
- **WHEN** an Asset is registered with a GUID and descriptor path
- **THEN** resolving that GUID returns the descriptor virtual path

#### Scenario: Path alone is not the durable public identity
- **WHEN** a descriptor file is moved and the registry is updated for the same GUID
- **THEN** Asset References using that GUID still resolve to the Asset

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

### Requirement: Asset Reference uses GUID
Cross-Asset links SHALL be stored as the target Asset’s GUID. Display and migration MAY use paths. Legacy path-only references SHALL be resolvable via the registry when possible.

#### Scenario: Scene mesh field stores GUID
- **WHEN** a Scene Asset is saved with a mesh assignment
- **THEN** the persisted mesh reference is the Mesh Asset GUID

#### Scenario: Legacy path mesh migrates
- **WHEN** a Scene document loads a path-only mesh field that maps to a registered descriptor
- **THEN** the reference resolves to that Asset’s GUID and a subsequent save persists the GUID

### Requirement: Scene is a first-class Asset
A Scene document (`.scene.asset`) SHALL include a GUID, SHALL be registered in the Asset Registry, and SHALL participate as a graph root for dependencies.

#### Scenario: New scene has guid
- **WHEN** a new Scene Asset is created or first saved under this pipeline
- **THEN** the document contains a GUID and the registry has an entry for it

#### Scenario: Existing scene without guid is upgraded
- **WHEN** a legacy `.scene.asset` without guid is opened
- **THEN** a GUID is allocated, written into the document (on save or upgrade path), and registered

