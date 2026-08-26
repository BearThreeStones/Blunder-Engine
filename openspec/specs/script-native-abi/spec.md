# script-native-abi Specification

## Purpose
Managed ScriptHost/Api consumes a native-registered C-ABI function-pointer table so Object/Behaviour/lifecycle calls target exactly one ObjectDB per process.

## Requirements

### Requirement: Native-registered C-ABI table for managed calls
Blunder.Api SHALL invoke Object, Behaviour, property, and lifecycle C-ABI operations through a native-registered function-pointer table rather than loading a second `blunder_engine_c` image via process-default `DllImport` when a script host is running in-process with `engine_runtime`.

#### Scenario: Register before use
- **WHEN** ScriptHost receives a complete non-null C-ABI function-pointer table from native DotNetHost start
- **THEN** subsequent Blunder.Api native calls use those pointers

#### Scenario: Call before register fails clearly
- **WHEN** Blunder.Api invokes a C-ABI operation before the table is registered
- **THEN** the call fails with a clear error (or throws) and MUST NOT silently `DllImport` a second ObjectDB image

#### Scenario: Editor and managed share one ObjectDB
- **WHEN** the editor process registers pointers from its process-linked C-ABI and attaches a Behaviour to a scene ObjectId via ScriptHost
- **THEN** native `ObjectDB::get` for that id observes the Behaviour peer and frame `LifecycleDispatch::invokeTick` reaches the managed Behaviour

### Requirement: Message entries on the NativeAbi table
BlunderNativeAbi SHALL include function pointers for message register, message send, message set-hook, and message clear-hook. Blunder.Api completeness checks SHALL require these entries to be non-null after registration. ABI version SHALL be at least 4 when these entries ship.

#### Scenario: Completeness includes message APIs
- **WHEN** a host fills BlunderNativeAbi from process or module after this change
- **THEN** message_register, message_send, message_set_hook, and message_clear_hook are non-null and engine_abi_version returns >= 4

#### Scenario: Managed Send uses registered pointers
- **WHEN** Blunder.Api Message.Send runs after RegisterNativeAbi
- **THEN** the call goes through the registered message_send pointer (not a second DllImport ObjectDB)

### Requirement: NativeAbi includes log
BlunderNativeAbi SHALL include a non-null function pointer for the Console log C-ABI entry after registration. Completeness checks SHALL require it. ABI version SHALL be at least 11 when this entry ships.

#### Scenario: Completeness includes log
- **WHEN** a host fills BlunderNativeAbi from process or module after this change
- **THEN** the log pointer is non-null and `engine_abi_version` returns >= 11

#### Scenario: Debug uses the registered pointer
- **WHEN** `Debug.Log` runs after RegisterNativeAbi
- **THEN** the call goes through the registered log pointer (not a second DllImport ObjectDB)

### Requirement: NativeAbi includes AnimationTree Clip Play
BlunderNativeAbi SHALL include a non-null function pointer for the AnimationTree Clip Play C-ABI entry after registration. Completeness checks SHALL require it. ABI version SHALL be at least 12 when this entry ships.

#### Scenario: Completeness includes Clip Play
- **WHEN** a host fills BlunderNativeAbi from process or module after this change
- **THEN** the Clip Play pointer is non-null and `engine_abi_version` returns >= 12

#### Scenario: Managed Play uses the registered pointer
- **WHEN** `AnimationTree.Play` runs after RegisterNativeAbi
- **THEN** the call goes through the registered Clip Play pointer (not a second DllImport ObjectDB)
