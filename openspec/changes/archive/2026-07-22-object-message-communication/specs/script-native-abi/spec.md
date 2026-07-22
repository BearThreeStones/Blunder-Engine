## ADDED Requirements

### Requirement: Message entries on the NativeAbi table
BlunderNativeAbi SHALL include function pointers for message register, message send, message set-hook, and message clear-hook. Blunder.Api completeness checks SHALL require these entries to be non-null after registration. ABI version SHALL be at least 4 when these entries ship.

#### Scenario: Completeness includes message APIs
- **WHEN** a host fills BlunderNativeAbi from process or module after this change
- **THEN** message_register, message_send, message_set_hook, and message_clear_hook are non-null and engine_abi_version returns >= 4

#### Scenario: Managed Send uses registered pointers
- **WHEN** Blunder.Api Message.Send runs after RegisterNativeAbi
- **THEN** the call goes through the registered message_send pointer (not a second DllImport ObjectDB)
