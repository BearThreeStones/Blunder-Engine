## ADDED Requirements

### Requirement: Import registers Intermediate Assets
Import SHALL allocate a GUID, write an Asset Descriptor under the Assets root, place Intermediate data under the Resources root (non-Source), and register the Asset. Import SHALL NOT be treated as Cook. Intermediate exchange formats already usable by the engine (glTF/GLB and supported images) SHALL Import without Source Export.

#### Scenario: Import glTF creates descriptor and Resources copy
- **WHEN** the user Imports an external glTF into a chosen Assets folder
- **THEN** a mesh Asset Descriptor with a new GUID exists under Assets, Intermediate data exists under Resources, and the registry maps the GUID

#### Scenario: Import is not Cook
- **WHEN** Import completes successfully
- **THEN** a Final MAY be produced later by Cook or Fast Path load, but Import success does not require a fresh Final to exist

### Requirement: Source Export for Assimp whitelist
When Import input is FBX or OBJ (v1 Assimp whitelist), Import SHALL run Source Export: convert to Intermediate glTF under Resources, archive the original under the Source root, write the Asset Descriptor pointing Cook/Fast Path at the Intermediate, and record the archived Source path for Reimport.

#### Scenario: Import FBX dual-writes Source and Intermediate
- **WHEN** the user Imports an FBX file
- **THEN** the original is stored under the Source root, a glTF Intermediate exists under Resources, and the descriptor references the Intermediate for load/Cook

#### Scenario: Unsupported Source type is rejected or deferred
- **WHEN** the user Imports a `.blend` file in v1
- **THEN** automatic Source Export does not silently succeed as a cooked Asset path (copy-to-Source-only or clear error is acceptable; no claim of full pipeline support)

### Requirement: Reimport
Reimport SHALL refresh an existing Asset: if an archived Source path exists, re-run Source Export over Intermediate; otherwise re-apply import settings against existing Intermediate; then invalidate Finals and dependents. Reimport SHALL preserve the Asset GUID.

#### Scenario: Reimport preserves GUID
- **WHEN** Reimport runs for an Asset
- **THEN** the Asset GUID is unchanged and dependents still resolve

#### Scenario: Reimport from archived Source
- **WHEN** Reimport runs for an Asset with an archived Source FBX
- **THEN** Intermediate glTF is regenerated from that Source and Finals are marked stale
