# animation-clip-independent-import Specification

## Purpose
TBD - created by archiving change animation-clip-independent-import. Update Purpose after archive.
## Requirements
### Requirement: AnimationClip Intermediate is Mesh-independent
Companion Animation glTF Intermediate bodies SHALL be stored under `Resources/Animations/<stem>/` (organization only). Import SHALL NOT place companion Intermediate under `Models/{mesh}/companions/` or `Models/_standalone_companions/` as the product layout. AnimationClip descriptors SHALL own the Intermediate `source` used for Reimport.

#### Scenario: Companion Intermediate under Animations
- **WHEN** Import registers clips from an accepted companion glTF with stem `LOOP-chocomel-idle`
- **THEN** the exchange Intermediate body lives under `resources/Animations/LOOP-chocomel-idle/` (or equivalent stem folder) and the Clip descriptor `source` points at that body or the extracted clip YAML path owned by the Clip

#### Scenario: No Mesh companions folder
- **WHEN** Import completes a host+companion multi-select batch
- **THEN** no new Intermediate files are required under `resources/Models/{host}/companions/`

### Requirement: No Mesh packaging list for companions
Import SHALL NOT write `companion_animation_sources` on Mesh descriptors. The Asset Dependency Graph SHALL NOT gain a Mesh→AnimationClip packaging edge from companion Import.

#### Scenario: Mesh YAML has no companion_animation_sources
- **WHEN** a skinned Mesh is Imported with companions in the same batch
- **THEN** the Mesh descriptor does not persist a `companion_animation_sources` list required for Clip lifetime or Reimport

### Requirement: Batch and near-disk are gestures only
Multi-select and near-disk discovery MAY Import Mesh and Companion Animation glTFs in one gesture. They SHALL register independent Mesh and/or AnimationClip Assets. They SHALL NOT persist Mesh↔Clip packaging links. When a batch contains exactly one skinned mesh host and accepted companions, Import SHALL warn on skeleton bone-name mismatch against that host and SHALL still register the Clip. Companion-only batches SHALL NOT invent a Mesh host. Import SHALL NOT auto-fill AnimationPlayer name→GUID maps as part of packaging.

#### Scenario: Multi-select registers independent clips
- **WHEN** the user multi-selects `Chocomel.gltf` plus idle and walk companions with Import Animations enabled
- **THEN** one Mesh Asset and stem-named AnimationClip Assets are registered without Mesh-owned companion packaging metadata

#### Scenario: Near-disk registers independent clips
- **WHEN** a single mesh glTF is Imported and near-disk discovery finds accepted companions
- **THEN** AnimationClip Assets are registered as independent Assets and the Mesh descriptor is not given a companion packaging list

#### Scenario: Bone mismatch warns
- **WHEN** a same-batch companion’s tracks do not overlap the skinned host skeleton bone names
- **THEN** Import logs a warning and still registers the AnimationClip Asset

### Requirement: Independent Reimport
Reimport of a Mesh Asset SHALL refresh only that Mesh’s Intermediate and SHALL NOT re-extract AnimationClips via former companion packaging lists. Reimport of an AnimationClip Asset SHALL refresh that Clip from its own descriptor `source` and SHALL preserve the Clip GUID when identity is stable.

#### Scenario: Mesh Reimport does not refresh clips
- **WHEN** Reimport runs for a Mesh that was historically imported with companions
- **THEN** companion-derived AnimationClip Assets are not modified solely because the Mesh was Reimported

#### Scenario: Clip Reimport preserves GUID
- **WHEN** Reimport runs for an AnimationClip with a valid `source`
- **THEN** Intermediate clip data is refreshed and the Clip GUID is unchanged

### Requirement: Mesh delete does not cascade clips
Deleting a Mesh Asset SHALL NOT delete AnimationClip Assets that were Imported in the same batch or discovered near-disk. Orphaned Intermediate under legacy companion folders is handled by migration, not by cascade delete.

#### Scenario: Delete Mesh leaves clips
- **WHEN** the user deletes `Chocomel.mesh.yaml` and idle/walk Clip descriptors exist
- **THEN** the Clip descriptors remain registered unless separately deleted

### Requirement: Legacy companion layout migration
The engine SHALL provide a one-shot migration that moves Intermediate bodies from `Models/**/companions/` and `Models/_standalone_companions/` into `Resources/Animations/<stem>/`, updates Clip descriptor `source` paths as needed, clears obsolete `companion_animation_sources` from Mesh descriptors, and preserves AnimationClip GUIDs.

#### Scenario: Standalone companions folder migrates
- **WHEN** migration runs on a project that has `resources/Models/_standalone_companions/LOOP-chocomel-idle.gltf`
- **THEN** that body is available under `resources/Animations/LOOP-chocomel-idle/` (or equivalent) and Clip Assets keep their GUIDs

