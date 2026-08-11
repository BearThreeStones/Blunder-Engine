## ADDED Requirements

### Requirement: Companion-only glTF Imports as AnimationClip Assets
When Import receives one or more companion Animation glTF/GLB files (animations present and meshes=0, skins allowed) without a skinned Mesh host in the same batch, and Import Animations is enabled, Import SHALL register AnimationClip Assets named from the companion file stem and SHALL NOT invent a Mesh Asset for those companions.

#### Scenario: Single LOOP companion Import creates clip
- **WHEN** the user Imports only `LOOP-chocomel-idle.gltf` (companion-accepted) with Import Animations enabled into the Content Browser selected folder
- **THEN** an AnimationClip descriptor such as `{selectedFolder}/LOOP-chocomel-idle.animation.yaml` is registered and no Mesh descriptor is created for that file

#### Scenario: Two orphan companions Import both clips
- **WHEN** the user Imports `LOOP-chocomel-idle.gltf` and `LOOP-chocomel-walk.gltf` without a skinned host in the batch
- **THEN** both stem-named AnimationClip Assets are registered

#### Scenario: Companion-only Import does not create Mesh
- **WHEN** a companion-only Import succeeds
- **THEN** Content Browser does not gain a `.mesh.yaml` for the companion path

### Requirement: Host pairing path unchanged
When a batch contains exactly one skinned Mesh host plus companions, Import SHALL still attach companions to that host (Intermediate companion sources + clip extract) as before.

#### Scenario: Multi-select host plus companions still pairs
- **WHEN** the user multi-selects `Chocomel.gltf` with both LOOP companions
- **THEN** one Mesh Asset is created/updated with companion Intermediate sources and clips are registered (standalone orphan path is not used for those paired companions)
