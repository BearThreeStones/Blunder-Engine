# asset-import Specification

## Purpose
TBD - created by archiving change asset-pipeline-pull. Update Purpose after archive.
## Requirements
### Requirement: Import registers Intermediate Assets
Import SHALL allocate a GUID, write an Asset Descriptor under the Assets root, place Intermediate data under the Resources root (non-Source), and register the Asset. Import SHALL NOT be treated as Cook. COLLADA (`.dae`) and supported images SHALL Import as Intermediate-direct (copy body under Resources, no Source archive required). glTF and GLB SHALL NOT Import as Intermediate-direct; they SHALL run Source Export.

#### Scenario: Import COLLADA creates descriptor and Resources copy
- **WHEN** the user Imports an external `.dae` into a chosen Assets folder
- **THEN** a mesh Asset Descriptor with a new GUID exists under Assets, Intermediate `.dae` data exists under Resources, and the registry maps the GUID

#### Scenario: Import glTF is not Intermediate-direct
- **WHEN** the user Imports an external glTF or GLB
- **THEN** Import runs Source Export (archive under Source root + COLLADA Intermediate) rather than registering the glTF/GLB file as Intermediate `source`

#### Scenario: Import is not Cook
- **WHEN** Import completes successfully
- **THEN** a Final MAY be produced later by Cook or Fast Path load, but Import success does not require a fresh Final to exist

### Requirement: Source Export for Assimp whitelist
When Import input is FBX, OBJ, glTF, or GLB (v1 Assimp whitelist), Import SHALL run Source Export: convert to Intermediate COLLADA (`.dae`) under Resources, archive the original under the Source root, write the Asset Descriptor pointing Cook/Fast Path at the Intermediate, and record the archived Source path for Reimport.

#### Scenario: Import FBX dual-writes Source and Intermediate COLLADA
- **WHEN** the user Imports an FBX file
- **THEN** the original is stored under the Source root, a `.dae` Intermediate exists under Resources, and the descriptor references the Intermediate for load/Cook

#### Scenario: Import glTF dual-writes Source and Intermediate COLLADA
- **WHEN** the user Imports a glTF or GLB file
- **THEN** the original is stored under the Source root, a `.dae` Intermediate exists under Resources, and the descriptor `source` is the COLLADA path (not the glTF/GLB)

#### Scenario: Unsupported Source type is rejected or deferred
- **WHEN** the user Imports a `.blend` file in v1
- **THEN** automatic Source Export does not silently succeed as a cooked Asset path (clear reject with no Asset / Intermediate / Source dual-write)

### Requirement: Reimport
Reimport SHALL refresh an existing Asset: if an archived Source path exists, re-run Source Export into Intermediate as defined by current Intermediate rules; otherwise refresh from the Asset’s Intermediate `source` (Intermediate-direct), including AnimationClip Reimport from the Clip descriptor `source`; then invalidate Finals and dependents. Reimport SHALL preserve the Asset GUID. Reimport SHALL remain invocable manually and MAY be invoked by Asset Watch through Detection Action. Mesh Reimport SHALL NOT refresh AnimationClip Assets via companion packaging metadata.

#### Scenario: Reimport preserves GUID
- **WHEN** Reimport runs for an Asset
- **THEN** the Asset GUID is unchanged and dependents still resolve

#### Scenario: Reimport from archived Source
- **WHEN** Reimport runs for an Asset with an archived Source path
- **THEN** Intermediate is regenerated from that Source and Finals are marked stale

#### Scenario: Intermediate-direct Reimport
- **WHEN** Reimport runs for an Asset whose descriptor has Intermediate `source` and no usable archived Source refresh path
- **THEN** Intermediate-derived data for that Asset is refreshed from `source` and the GUID is unchanged

#### Scenario: Detection may invoke Reimport
- **WHEN** Detection Action confirms or auto-runs for attributed GUIDs
- **THEN** Reimport runs for those GUIDs through the same Reimport entry points as manual Reimport

### Requirement: Companion Import does not package under Mesh
When Import Animations is enabled and Companion Animation glTFs are Imported (multi-select, near-disk, or companion-only), Import SHALL register AnimationClip Assets whose Intermediate layout follows `Resources/Animations/<stem>/` and SHALL NOT require `companion_animation_sources` on any Mesh descriptor produced or updated in that Import.

#### Scenario: Host plus companions without Mesh packaging field
- **WHEN** Import receives one skinned mesh host and accepted companions with animations enabled
- **THEN** AnimationClip Assets are registered and the Mesh descriptor does not gain a durable `companion_animation_sources` packaging list

#### Scenario: Companion-only still independent
- **WHEN** Import receives only companion-accepted glTFs with animations enabled
- **THEN** AnimationClip Assets are registered under the selected Assets folder naming rules and no Mesh host is invented

### Requirement: Mesh Reimport is not Clip Reimport
Reimport for a Mesh Asset SHALL NOT use companion packaging metadata to refresh AnimationClip Assets. AnimationClip Reimport SHALL use the Clip descriptor’s own `source`.

#### Scenario: Mesh Reimport leaves clip GUIDs untouched
- **WHEN** Mesh Reimport completes for a skinned Mesh
- **THEN** existing AnimationClip Asset GUIDs and bodies are not required to change as a side effect of that Mesh Reimport

