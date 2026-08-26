## ADDED Requirements

### Requirement: NativeAbi includes AnimationTree Clip Play
BlunderNativeAbi SHALL include a non-null function pointer for the AnimationTree Clip Play C-ABI entry after registration. Completeness checks SHALL require it. ABI version SHALL be at least 12 when this entry ships.

#### Scenario: Completeness includes Clip Play
- **WHEN** a host fills BlunderNativeAbi from process or module after this change
- **THEN** the Clip Play pointer is non-null and `engine_abi_version` returns >= 12

#### Scenario: Managed Play uses the registered pointer
- **WHEN** `AnimationTree.Play` runs after RegisterNativeAbi
- **THEN** the call goes through the registered Clip Play pointer (not a second DllImport ObjectDB)
