## ADDED Requirements

### Requirement: NativeAbi includes log
BlunderNativeAbi SHALL include a non-null function pointer for the Console log C-ABI entry after registration. Completeness checks SHALL require it. ABI version SHALL be at least 11 when this entry ships.

#### Scenario: Completeness includes log
- **WHEN** a host fills BlunderNativeAbi from process or module after this change
- **THEN** the log pointer is non-null and `engine_abi_version` returns >= 11

#### Scenario: Debug uses the registered pointer
- **WHEN** `Debug.Log` runs after RegisterNativeAbi
- **THEN** the call goes through the registered log pointer (not a second DllImport ObjectDB)
