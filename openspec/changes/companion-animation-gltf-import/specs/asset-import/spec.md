## ADDED Requirements

### Requirement: Mesh Import attaches Companion Animation glTFs
When mesh Import runs with animations enabled, Import SHALL attach Companion Animation glTFs per the companion-animation-import capability (multi-select host pairing and/or near-disk discovery), in addition to extracting animations embedded in the mesh Intermediate. Companion attach SHALL NOT register companion files as Mesh Assets and SHALL NOT require a Godot AnimationLibrary.

#### Scenario: Embedded plus companion clips
- **WHEN** a mesh glTF embeds one animation and a companion supplies another
- **THEN** Import registers one Mesh Asset and AnimationClip Assets for both sources under the mesh stem

#### Scenario: Animations disabled skips companions
- **WHEN** mesh Import runs with animations disabled
- **THEN** Import does not attach Companion Animation glTFs for that Import
