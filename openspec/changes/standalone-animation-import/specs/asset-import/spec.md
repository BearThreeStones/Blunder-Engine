## ADDED Requirements

### Requirement: Orphan companions Import as clips when animations enabled
For `importExternalFiles`, companion glTF paths classified as orphans (no single unambiguous skinned host in the batch) SHALL be imported as AnimationClip sources when Mesh Import settings have animations enabled, instead of only warning and skipping.

#### Scenario: Orphan companion no longer hard-skipped
- **WHEN** Import Animations is enabled and the batch contains companion-only glTF orphans
- **THEN** Import produces successful AnimationClip `ImportResult` entries for those companions (subject to extract success)

#### Scenario: Animations disabled skips orphan clip Import
- **WHEN** Import Animations is disabled and the batch contains companion-only orphans
- **THEN** those orphans do not register AnimationClip Assets
