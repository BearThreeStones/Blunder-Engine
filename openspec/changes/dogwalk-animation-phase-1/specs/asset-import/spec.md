## MODIFIED Requirements

### Requirement: Import registers Intermediate Assets
Import SHALL allocate a GUID, write an Asset Descriptor under the Assets root, place Intermediate data under the Resources root (non-Source), and register the Asset. Import SHALL NOT be treated as Cook. glTF/GLB and supported images SHALL Import as Intermediate-direct (copy body under Resources; optional Source archive of the same bytes MAY be recorded). COLLADA (`.dae`) SHALL NOT Import as mesh Intermediate.

#### Scenario: Import glTF creates descriptor and Resources Intermediate
- **WHEN** the user Imports an external glTF or GLB into a chosen Assets folder
- **THEN** a mesh Asset Descriptor with a new GUID exists under Assets, Intermediate glTF/GLB data exists under Resources, and the registry maps the GUID

#### Scenario: Import COLLADA is rejected as mesh Intermediate
- **WHEN** the user Imports an external `.dae` as a mesh
- **THEN** Import does not register COLLADA as mesh Intermediate (clear reject or non-mesh handling per product policy)

#### Scenario: Import is not Cook
- **WHEN** Import completes successfully
- **THEN** a Final MAY be produced later by Cook or Fast Path load, but Import success does not require a fresh Final to exist

### Requirement: Source Export for Assimp whitelist
When Import input is FBX or OBJ (v1 Assimp whitelist for non-glTF exchange), Import SHALL run Source Export: convert to Intermediate glTF/GLB under Resources, archive the original under the Source root, write the Asset Descriptor pointing Cook/Fast Path at the Intermediate glTF, and record the archived Source path for Reimport. glTF/GLB Import SHALL NOT require conversion through COLLADA.

#### Scenario: Import FBX dual-writes Source and Intermediate glTF
- **WHEN** the user Imports an FBX file
- **THEN** the original is stored under the Source root, a glTF/GLB Intermediate exists under Resources, and the descriptor references that Intermediate for load/Cook

#### Scenario: Import glTF does not produce COLLADA
- **WHEN** the user Imports a glTF or GLB file
- **THEN** Intermediate `source` is glTF/GLB under Resources and no COLLADA Intermediate is required for that Asset

#### Scenario: Unsupported Source type is rejected or deferred
- **WHEN** the user Imports a `.blend` file in v1
- **THEN** automatic Source Export does not silently succeed as a cooked Asset path (clear reject with no Asset / Intermediate / Source dual-write)

### Requirement: Reimport
Reimport SHALL refresh an existing Asset: if an archived Source path exists, re-run Source Export / refresh over Intermediate (producing glTF Intermediate for mesh exchange Sources); otherwise re-apply import settings against existing Intermediate; then invalidate Finals and dependents. Reimport SHALL preserve the Asset GUID. For skinned Sources, Reimport SHALL refresh AnimationClip YAML sidecars extracted from animations.

#### Scenario: Reimport preserves GUID
- **WHEN** Reimport runs for an Asset
- **THEN** the Asset GUID is unchanged and dependents still resolve

#### Scenario: Reimport from archived Source
- **WHEN** Reimport runs for an Asset with an archived Source FBX or OBJ
- **THEN** Intermediate glTF/GLB is regenerated from that Source and Finals are marked stale

#### Scenario: Reimport refreshes clip YAML
- **WHEN** Reimport runs for a skinned glTF that defines multiple animations
- **THEN** AnimationClip YAML Intermediates are refreshed and clip Asset GUIDs are preserved when clip identity is stable

## ADDED Requirements

### Requirement: Multi-animation glTF yields Mesh plus Clip Assets
When Importing a glTF/GLB that contains multiple animations, Import SHALL register one Mesh Asset (geometry + skin/bind as applicable) and one AnimationClip Asset per extracted animation, each with its own GUID. Clip Intermediate SHALL be readable YAML preserving Constant/Stepped and Linear interpolation from the source samplers.

#### Scenario: Two animations become two clips
- **WHEN** the user Imports a glTF with animations named idle and walk
- **THEN** one Mesh Asset and two AnimationClip Assets exist, and each clip’s YAML records its keys and interpolation

### Requirement: AnimationPlayer map auto-fill on character setup
When a skinned character Object is created or Import associates clips with an AnimationPlayer, the engine/editor SHALL auto-fill the player’s name→GUID map from extracted clip names. The map SHALL remain editable afterward in the Inspector.

#### Scenario: Auto-fill then edit
- **WHEN** Import/setup completes for a multi-clip character
- **THEN** AnimationPlayer contains name entries for those clips pointing at Clip GUIDs, and the author can add/remove/rename map entries without losing GUID durability of clip Assets
