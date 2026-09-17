# Spec Delta

## ADDED Requirements

### Requirement: NativeAbi includes physics query and Character Controller
BlunderNativeAbi SHALL include non-null function pointers for physics raycast, box/sphere/capsule shapecast, entity groups, and Character Controller MoveAndSlide after registration. Completeness checks SHALL require them. ABI version SHALL be at least 13 when these entries ship.

#### Scenario: Completeness includes physics APIs
- **WHEN** a host fills BlunderNativeAbi from process or module after this change
- **THEN** the physics query, group, and Character Controller pointers are non-null
- **AND** `engine_abi_version` returns >= 13

#### Scenario: Managed query uses the registered pointer
- **WHEN** `Physics` raycast runs after RegisterNativeAbi
- **THEN** the call goes through the registered physics query pointer (not a second DllImport ObjectDB)
