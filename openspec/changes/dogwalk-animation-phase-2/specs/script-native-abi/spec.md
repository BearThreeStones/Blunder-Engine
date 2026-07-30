## ADDED Requirements

### Requirement: Blunder.Api two-slot and TimeScale surface
Blunder.Api AnimationPlayer façade SHALL expose managed APIs for slot assignment, blendWeight, global TimeScale, and Play with fade, backed by the registered NativeAbi table. Completeness checks SHALL require the new NativeAbi entries to be non-null after registration when this ABI version ships.

#### Scenario: Managed SetSlot and blendWeight
- **WHEN** script code sets slot clips and blendWeight after RegisterNativeAbi
- **THEN** calls go through the registered C-ABI pointers (not a second DllImport ObjectDB)

#### Scenario: Completeness includes blend APIs
- **WHEN** a host fills BlunderNativeAbi after this change
- **THEN** the new two-slot / blendWeight / TimeScale / Play-with-fade entries are non-null and abi version reflects the bump
