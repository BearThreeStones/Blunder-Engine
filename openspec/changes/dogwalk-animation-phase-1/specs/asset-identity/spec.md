## MODIFIED Requirements

### Requirement: Content root roles
The Project SHALL keep four content roles: Assets root for Intermediate descriptors (and Scene documents); Resources root (non-Source) for Intermediate data bodies; Source root (`Resources/Source/`) for Source Assets only; Cooked cache (`.blunder/cooked/`) for Final Assets keyed by GUID. Mesh Intermediate data bodies SHALL be glTF/GLB (`.gltf` / `.glb`), including skinned meshes. Texture Intermediate data bodies SHALL remain image files. AnimationClip Intermediate data bodies SHALL be readable YAML sidecars. COLLADA (`.dae`) SHALL NOT be registered as mesh Intermediate.

#### Scenario: Descriptor not stored under Resources
- **WHEN** Import creates a mesh or texture Asset
- **THEN** the YAML descriptor is written under the Assets root and Intermediate data under Resources (non-Source)

#### Scenario: glTF Intermediate under Resources
- **WHEN** Import registers a mesh from glTF or GLB
- **THEN** Intermediate mesh data under Resources (non-Source) is glTF/GLB and the descriptor `source` refers to that Intermediate (not COLLADA)

#### Scenario: Mesh Intermediate is glTF
- **WHEN** a mesh Asset has a successful modern Intermediate `source`
- **THEN** that `source` path refers to a `.gltf` or `.glb` file under Resources (non-Source)

#### Scenario: AnimationClip Intermediate is YAML
- **WHEN** an AnimationClip Asset is registered from Import extraction
- **THEN** its Intermediate body is readable YAML under Resources (non-Source) and it has its own GUID descriptor under Assets

## ADDED Requirements

### Requirement: AnimationClip is a first-class Asset
An AnimationClip SHALL be a GUID-identified Asset distinct from Mesh Asset. Product references (AnimationPlayer maps, dependency graph) SHALL address clips by GUID.

#### Scenario: Clip has own GUID
- **WHEN** Import extracts an animation from a multi-clip glTF
- **THEN** each clip Asset has its own GUID and descriptor separate from the Mesh Asset GUID
