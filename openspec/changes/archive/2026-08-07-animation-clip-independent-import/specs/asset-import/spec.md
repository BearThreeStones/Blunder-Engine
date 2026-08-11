## ADDED Requirements

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
